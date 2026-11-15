#!/usr/bin/env python3
"""One-command gate for the DOS side of the decompilation.

    python3 tools/gate.py check [--serial] [--only game|u|end] [--keep DIR]

Rebuilds the three programs with the original tools in a headless DOSBox, by calling the
existing scripts, and requires each to be identical to the original:

    GAME.EXE  from SRC/      tools/dosbuild.py
    U.EXE     from SRC_U/    tools/u/ubuild.py
    END.EXE   from SRC_END/  U6_PROG=end tools/u/ubuild.py

It prints one line per program (sha1 of the build, PASS/FAIL), the total time, and exits
non-zero if any build differs or if an original, the toolchain or a DOSBox is missing.

Environment honoured (the scripts' own): U6_GAMEEXE, U6_UEXE, U6_ENDEXE (the originals),
U6_DOSTOOLS (folder with turboc20/ and masm300/). Also: U6_DOSBOX (path of a DOSBox-X or
DOSBox to use).

Accommodations made here so that nothing has to be set up by hand:
  * snap DOSBox-X (WSL/Ubuntu) hangs headless unless started with `-defaultconf -fastlaunch`
    and has a private /tmp: a wrapper script adds the options, and TMPDIR is a folder under
    the home directory;
  * MASM 3.0 is on a floppy image and the scripts extract it with mtools' `mcopy`; where
    mcopy is missing (Windows) MASM.EXE is read out of the image here and a staged tools
    folder is used through U6_DOSTOOLS.
Nothing is written into the repository (no bytecode, no scratch files), except with --keep.

The three builds use separate tempfile directories, so they run in parallel by default.
"""
import argparse, concurrent.futures as cf, os, re, shutil, stat, struct, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WIN = os.name == 'nt'

PROGS = [
    # key, display, exe, env var for the original, script, extra env
    ('game', 'GAME.EXE', 'U6_GAMEEXE', os.path.join(HERE, 'dosbuild.py'), {}),
    ('u', 'U.EXE', 'U6_UEXE', os.path.join(HERE, 'u', 'ubuild.py'), {}),
    ('end', 'END.EXE', 'U6_ENDEXE', os.path.join(HERE, 'u', 'ubuild.py'), {'U6_PROG': 'end'}),
]


# the sha1 each build must have (README.md, "Verified with the real compiler"; GAME.EXE's is
# of the link before EXEPACK, as tools/game.sha1): a build that equals whatever file was
# given as the original, but not this, is not a pass
EXPECT = {
    'game': '5129af96d4ba63b7f84c663e14e3a3b90fd524a3',
    'u': 'b6cde9fb239d56656777ad52adbe1324315b65ac',
    'end': '91868fa6b5e8f5c2a85d2b5393a74ab2ae268005',
}


class Missing(Exception):
    pass


def find_original(exe, envvar):
    """the same places the build scripts look in"""
    if os.environ.get(envvar) and not os.path.exists(os.environ[envvar]):
        raise Missing('%s is set to %s, which does not exist' % (envvar, os.environ[envvar]))
    cands = [os.environ.get(envvar), os.path.join(ROOT, 'gamedata', exe),
             os.path.join(ROOT, '..', 'gamedata', exe), '/home/claude/gamedata/' + exe]
    for p in cands:
        if p and os.path.exists(p):
            return os.path.abspath(p)
    raise Missing('original %s not found (looked for %s; set %s)' % (
        exe, ', '.join(p for p in cands if p), envvar))


def fat12_extract(img, name):
    """read one file from the root directory of a FAT12 floppy image (no mtools needed)"""
    d = open(img, 'rb').read()
    bps, spc, rsv, nfat, nroot, _, _, spf = struct.unpack_from('<HBHBHHBH', d, 11)
    root = (rsv + nfat * spf) * bps
    data = root + nroot * 32
    fat = d[rsv * bps:(rsv + spf) * bps]
    want = (name.split('.')[0].ljust(8) + name.split('.')[1].ljust(3)).upper().encode()
    for i in range(nroot):
        e = d[root + i * 32:root + i * 32 + 32]
        if e[:11] == want:
            cl, size = struct.unpack_from('<H', e, 26)[0], struct.unpack_from('<I', e, 28)[0]
            out = b''
            while 2 <= cl < 0xFF8:
                o = data + (cl - 2) * spc * bps
                out += d[o:o + spc * bps]
                v = struct.unpack_from('<H', fat, cl * 3 // 2)[0]
                cl = (v >> 4) if cl & 1 else (v & 0xFFF)
            return out[:size]
    raise Missing('%s not found in %s' % (name, img))


def check_toolchain(tools, scratch, env):
    """verify turboc20/masm300; returns the tools folder the scripts should use"""
    tc, md = os.path.join(tools, 'turboc20'), os.path.join(tools, 'masm300')
    if not os.path.isdir(tc):
        raise Missing('Turbo C 2.0 not found: %s is not a folder (put it in tools/turboc20 or set U6_DOSTOOLS)' % tc)
    have = {n.upper() for n in os.listdir(tc)}
    for need in ('TCC.EXE', 'TLINK.EXE', 'MAKE.EXE', 'TLIB.EXE'):
        if need not in have:
            raise Missing('Turbo C 2.0 incomplete: %s lacks %s' % (tc, need))
    if not os.path.isdir(md):
        raise Missing('MASM 3.0 not found: %s is not a folder (put it in tools/masm300 or set U6_DOSTOOLS)' % md)
    names = os.listdir(md)
    if any(n.upper() == 'MASM.EXE' for n in names):
        return tools
    imgs = [n for n in names if n.lower().endswith('.img')]
    if not imgs:
        raise Missing('MASM 3.0 not found: %s has neither MASM.EXE nor a floppy image' % md)
    if shutil.which('mcopy', path=env.get('PATH')):
        return tools
    # no mtools: stage a tools folder with MASM.EXE already extracted
    stage = os.path.join(scratch, 'tools')
    os.makedirs(os.path.join(stage, 'masm300'))
    shutil.copytree(tc, os.path.join(stage, 'turboc20'))
    open(os.path.join(stage, 'masm300', 'MASM.EXE'), 'wb').write(
        fat12_extract(os.path.join(md, imgs[0]), 'MASM.EXE'))
    return stage


def find_dosbox(scratch, env):
    """returns (path, note); on snap DOSBox-X puts a wrapper in scratch and sets TMPDIR"""
    cands = [os.environ.get('U6_DOSBOX')]
    if WIN:
        cands.append(os.path.join(HERE, 'Dosbox-X', 'dosbox-x.exe'))
    cands += [shutil.which('dosbox-x'), '/snap/bin/dosbox-x', shutil.which('dosbox')]
    path = next((c for c in cands if c and os.path.exists(c)), None)
    if not path:
        raise Missing('no DOSBox found (tried U6_DOSBOX, tools/Dosbox-X, dosbox-x and dosbox on PATH, /snap/bin/dosbox-x)')
    if not WIN and (path.startswith('/snap/') or os.path.realpath(path).startswith('/snap/')):
        wdir = os.path.join(scratch, 'bin')
        os.makedirs(wdir)
        w = os.path.join(wdir, 'dosbox-x')
        open(w, 'w').write('#!/bin/sh\nexec timeout 900 %s -defaultconf -fastlaunch "$@"\n' % path)
        os.chmod(w, os.stat(w).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        env['PATH'] = wdir + os.pathsep + env.get('PATH', '')
        return w, 'snap DOSBox-X via wrapper (-defaultconf -fastlaunch)'
    if 'dosbox-x' in os.path.basename(path).lower():
        # the scripts look for it on PATH (ubuild) or take it as an argument (dosbuild)
        env['PATH'] = os.path.dirname(os.path.abspath(path)) + os.pathsep + env.get('PATH', '')
    return path, path


def run_one(prog, original, dosbox, tools, env, keep=None, timeout=2400):
    key, exe, envvar, script, extra = prog
    e = dict(env, **extra)
    e[envvar] = original
    cmd = [sys.executable, script]
    if keep:
        cmd += ['--keep', os.path.join(keep, key)]
    if key == 'game':
        cmd += ['--dosbox', dosbox, '--original', original]
    t0 = time.time()
    try:
        p = subprocess.run(cmd, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout,
                           stdin=subprocess.DEVNULL, cwd=tempfile.gettempdir())
        out, rc = p.stdout.decode('latin-1'), p.returncode
    except subprocess.TimeoutExpired as x:
        out, rc = (x.stdout or b'').decode('latin-1') + '\ntimed out after %d s' % timeout, -1
    shas = re.findall(r'sha1 ([0-9a-f]{40})', out)
    ok = rc == 0 and bool(re.search(r'^identical to the original', out, re.M)) and bool(shas)
    if ok and shas[0] != EXPECT[key]:
        ok = False
        out += '\ngate: the build is sha1 %s, expected %s' % (shas[0], EXPECT[key])
    return dict(exe=exe, ok=ok, sha=shas[0] if shas else '-', out=out, rc=rc, secs=time.time() - t0)


def check(args):
    t0 = time.time()
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1')
    progs = [p for p in PROGS if not args.only or p[0] == args.only]
    base = None
    if not WIN:  # under the home folder, where a snap DOSBox can see it
        base = os.path.expanduser('~/tmp')
        os.makedirs(base, exist_ok=True)
    scratch = tempfile.mkdtemp(prefix='u6gate-', dir=base)
    try:
        try:
            originals = {p[0]: find_original(p[1], p[2]) for p in progs}
            tools = check_toolchain(os.environ.get('U6_DOSTOOLS') or os.path.join(ROOT, 'tools'), scratch, env)
            dosbox, note = find_dosbox(scratch, env)
        except Missing as m:
            print('gate: MISSING: %s' % m)
            return 2
        if tools != (os.environ.get('U6_DOSTOOLS') or os.path.join(ROOT, 'tools')):
            env['U6_DOSTOOLS'] = tools
        print('gate: DOSBox: %s' % note)
        for p in progs:
            print('gate: original %-8s %s' % (p[1], originals[p[0]]))
        env['TMPDIR'] = scratch   # the scripts' tempfile folders go under it
        keep = os.path.abspath(args.keep) if args.keep else None
        if keep:
            for p in progs:
                shutil.rmtree(os.path.join(keep, p[0]), ignore_errors=True)
        sys.stdout.flush()
        if args.serial or len(progs) == 1:
            res = [run_one(p, originals[p[0]], dosbox, tools, env, keep) for p in progs]
        else:
            with cf.ThreadPoolExecutor(len(progs)) as ex:
                res = list(ex.map(lambda p: run_one(p, originals[p[0]], dosbox, tools, env, keep), progs))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    bad = 0
    for r in res:
        print('%-9s sha1 %s  %s  (%.0f s)' % (r['exe'], r['sha'], 'PASS' if r['ok'] else 'FAIL', r['secs']))
        if not r['ok']:
            bad += 1
            tail = r['out'].strip().splitlines()[-15:]
            print('  exit code %s; last lines of its output:' % r['rc'])
            for l in tail:
                print('    ' + l)
    print('gate: %s: %d of %d programs identical to the originals, total %.0f s' % (
        'FAIL' if bad else 'PASS', len(res) - bad, len(res), time.time() - t0))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('check', help='rebuild GAME.EXE, U.EXE and END.EXE and compare with the originals')
    c.add_argument('--serial', action='store_true', help='one build at a time (default: the three in parallel)')
    c.add_argument('--keep', metavar='DIR', help='also keep each build folder in DIR/game, DIR/u, DIR/end '
                   '(the port tests read DIR/game/GAME.EXE and GAME.MAP)')
    c.add_argument('--only', choices=['game', 'u', 'end'], help='just one program')
    a = ap.parse_args()
    sys.exit(check(a))


if __name__ == '__main__':
    main()
