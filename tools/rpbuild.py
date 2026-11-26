#!/usr/bin/env python3
"""The replay DOS build of GAME.EXE (PLAN-exact-port.md, Phase 1; docs/replay.md).

    python3 tools/rpbuild.py [--out DIR] [--force] [--keep-tree]

Exhume's `tools/replay.py build` runs this ([replay] build in exhume.toml), because Exhume's own
replay build is its modding link, which this project does not have: Ultima VI's replay build
swaps two assembly modules (OSILIB/MOUSERP and DELAYRP for MOUSE and DELAY), adds STACKRP and
Exhume's runtime/replay/rpfar.asm (assembled by MASM 3.00, which has no /D), and links with
TLINK from a link file, as SRC/MAKEFILE does.

It stages the tree as tools/dosbuild.py does (Turbo C 2.0, MASM 3.00 read out of its floppy
image, the headers Turbo C reads given CRLF line ends: Exhume's portable.h, hookgame.h, rpgame.h,
replay.c, rpfar.asm), then in one headless DOSBox-X session:
  1. assembles OSILIB (dosbuild's modules plus MOUSERP, DELAYRP, STACKRP) and makes the normal
     GAME.EXE with SRC/MAKEFILE, then relinks those objects with /m into GAMEREF.MAP;
  2. compiles every C file again with -DREPLAY into RP\\ (the three huge-model data files with
     their own switches), replay.c with -DREPLAY -DRP_FAR_STORAGE, assembles rpfar.asm, and
     links RGAME.EXE / RGAME.MAP from u6.lnk with RP\\ objects, OSILIB\\mouserp and delayrp in
     place of mouse and delay, and RP\\replay RP\\rpfar OSILIB\\stackrp after the libraries.
Then it checks the normal GAME.EXE against tools/game.sha1 and compares the two maps: _DATA, _BSS,
_BSSEND and every DGROUP public must keep its DGROUP offset (the harness keeps nothing in DGROUP,
so the game's near pointers are the real game's). It fails, and leaves no GAME.EXE in --out, if
anything moved.

Output (--out, default build/dos/replay): GAME.EXE (the replay build, the name the game runs
under; also RGAME.EXE), RGAME.MAP, GAMEREF.MAP, BUILD.LOG, build.sha1; with --keep-tree the
whole staged tree in tree/ (replay.c's assembly for a stack count, the objects). The build is
skipped when build.sha1 names the same inputs (every file under SRC/, the runtime files it
stages, this script); --force builds anyway.

Environment: U6_DOSBOX (a DOSBox-X), U6_DOSTOOLS (folder with turboc20/ and masm300/), as the gate.
"""
import argparse, hashlib, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gate  # noqa: E402  (find_dosbox, fat12_extract, EXPECT)

GAME_SHA1 = gate.EXPECT['game']
OSILIB = ['OSI_FILE', 'KBD', 'INFLATE', 'MOUSE', 'LOW', 'DELAY', 'DELAY2', 'RAND',
          'SOUND1', 'SOUND2', 'SOUND3', 'SOUND4', 'SOUND5', 'MOUSERP', 'DELAYRP', 'STACKRP']
C_MEDIUM = ['main', 'gameloop', 'console', 'map', 'visible', 'object', 'status', 'talk', 'talkdr',
            'spell', 'npctrack', 'combat', 'objcmd', 'egg', 'music', 'gfx', 'rest', 'disk', 'gamestat']
C_HUGE = ['padding', 'monsdata', 'msgs']
RUNTIME = {  # staged name: path under exhume/runtime, all with CRLF line ends
    'PORTABLE.H': ('include', 'portable.h'),
    'REPLAY.C': ('replay', 'replay.c'),
    'RPFAR.ASM': ('replay', 'rpfar.asm'),
}
CRLF_SRC = ['hookgame.h', 'rpgame.h']   # git stores these with LF


def crlf(data):
    return data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')


def inputs_key(src, rt):
    """sha1 over everything the build reads but the tools: SRC/, the staged runtime files, this script"""
    h = hashlib.sha1()
    files = []
    for d, _, fs in os.walk(src):
        for f in fs:
            if not f.upper().endswith(('.OBJ', '.EXE', '.LIB', '.MAP', '.LST')):
                files.append(os.path.join(d, f))
    files += [os.path.join(rt, *p) for p in RUNTIME.values()] + [os.path.abspath(__file__)]
    for p in sorted(files):
        h.update(os.path.relpath(p, ROOT).replace('\\', '/').encode() + b'\0')
        h.update(open(p, 'rb').read())
    return h.hexdigest()


def parse_map(path):
    segs, pubs = {}, {}
    txt = open(path, encoding='latin-1').read()
    for m in re.finditer(r'^\s*([0-9A-F]{5})H\s+([0-9A-F]{5})H\s+([0-9A-F]{5})H\s+(\S+)\s+(\S+)', txt, re.M):
        segs[m.group(4)] = (int(m.group(1), 16), int(m.group(3), 16), m.group(5))
    byval = txt.split('Publics by Value')[1] if 'Publics by Value' in txt else ''
    for m in re.finditer(r'^\s*([0-9A-F]{4}):([0-9A-F]{4})\s+(?:Abs\s+|Idle\s+)?(\S+)', byval, re.M):
        pubs[m.group(3)] = (int(m.group(1), 16), int(m.group(2), 16))
    return segs, pubs


def compare_maps(ref, rp):
    """the DGROUP check: the number of things that moved (0 is a pass)"""
    s0, p0 = parse_map(ref)
    s1, p1 = parse_map(rp)
    base0, base1 = s0['_DATA'][0], s1['_DATA'][0]   # DGROUP's base: _DATA is its first segment
    bad = 0
    for name in ('_DATA', '_BSS', '_BSSEND'):
        a = (s0[name][0] - base0, s0[name][1])
        b = (s1[name][0] - base1, s1[name][1])
        print('%-8s normal off %04X len %04X   replay off %04X len %04X  %s' % (
            name, a[0], a[1], b[0], b[1], 'same' if a == b else 'DIFFERENT'))
        bad += a != b
    g0, g1 = base0 >> 4, base1 >> 4
    d0 = {n: o for n, (s, o) in p0.items() if s == g0}
    d1 = {n: o for n, (s, o) in p1.items() if s == g1}
    moved = [n for n in d0 if d1.get(n) != d0[n]]
    extra = sorted(set(d1) - set(d0))
    print('DGROUP publics: normal %d, replay %d; same offset %d, moved or missing %d, only in replay %d %s' % (
        len(d0), len(d1), len(d0) - len(moved), len(moved), len(extra), extra))
    for n in moved[:20]:
        print('  moved: %s %04X -> %s' % (n, d0[n], '%04X' % d1[n] if n in d1 else 'missing'))
    print('DGROUP paragraph in RGAME.EXE: %04Xh' % g1)
    return bad + len(moved) + len(extra) + (not d0)


def batch_lines():
    lib = '\\TOOLS\\TURBOC20\\LIB'
    inc = '-I\\TOOLS\\TURBOC20\\INCLUDE'
    L = ['@echo off', 'set PATH=\\TOOLS\\TURBOC20', 'cd \\S\\OSILIB']
    for n in OSILIB:
        L.append('\\TOOLS\\MASM300 /Mx %s; >> \\BUILD.LOG' % n)
    L.append('TLIB OSISND.LIB +SOUND1 +SOUND2 +SOUND3 +SOUND4 +SOUND5 >> \\BUILD.LOG')
    L.append('cd \\S')
    L.append('make game.exe >> \\BUILD.LOG')
    L.append('echo ==== normal relink /m >> \\BUILD.LOG')
    L.append('tlink /m padding monsdata msgs %s\\c0m @u6.lnk,gameref.exe,gameref.map,%s\\CM >> \\BUILD.LOG' % (lib, lib))
    L.append('echo ==== replay compile >> \\BUILD.LOG')
    for n in C_MEDIUM:
        L.append('tcc %s -d -mm -p -c -Z -O -DREPLAY -nRP %s.c >> \\BUILD.LOG' % (inc, n))
    for n in C_HUGE:
        L.append('tcc %s -d -mh -p -c -Z -O -DREPLAY -nRP %s.c >> \\BUILD.LOG' % (inc, n))
    L.append('tcc %s -d -mm -p -c -Z -O -DREPLAY -DRP_FAR_STORAGE -nRP replay.c >> \\BUILD.LOG' % inc)
    L.append('\\TOOLS\\MASM300 /Mx RPFAR,RP\\RPFAR; >> \\BUILD.LOG')
    # replay.c and RP_SHUTDOWN's game code as assembly too, for a static stack count (the
    # harness's budget is 256 bytes a hook: PLAN-exact-port.md section 9)
    L.append('tcc %s -d -mm -p -S -Z -O -DREPLAY -DRP_FAR_STORAGE -nRP replay.c >> \\BUILD.LOG' % inc)
    for n in ('main', 'object', 'music', 'disk'):
        L.append('tcc %s -d -mm -p -S -Z -O -DREPLAY -nRP %s.c >> \\BUILD.LOG' % (inc, n))
    L.append('echo ==== replay link >> \\BUILD.LOG')
    L.append('tlink /m RP\\padding RP\\monsdata RP\\msgs %s\\c0m @u6rp.lnk,rgame.exe,rgame.map,%s\\CM >> \\BUILD.LOG' % (lib, lib))
    return L


def replay_lnk(src):
    """u6.lnk with the replay build's objects and modules"""
    lnk = open(os.path.join(src, 'u6.lnk'), encoding='latin-1').read().replace('\r\n', '\n')
    for n in C_MEDIUM:
        lnk = re.sub(r'(?<![\\\w])%s(?=[\s+])' % n, 'RP\\\\' + n, lnk)
    want = [('OSILIB\\mouse ', 'OSILIB\\mouserp '), ('OSILIB\\delay+', 'OSILIB\\delayrp+'),
            ('OSILIB\\OSISND.LIB+\n', 'OSILIB\\OSISND.LIB+\nRP\\replay RP\\rpfar OSILIB\\stackrp+\n')]
    for old, new in want:
        if old not in lnk:
            sys.exit('rpbuild: SRC/u6.lnk has no %r any more; update replay_lnk()' % old.strip())
        lnk = lnk.replace(old, new)
    return lnk.replace('\n', '\r\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'dos', 'replay'))
    ap.add_argument('--force', action='store_true', help='build even when build.sha1 says it is up to date')
    ap.add_argument('--keep-tree', action='store_true', help='also keep the staged tree in OUT/tree')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    src = os.path.join(ROOT, 'SRC')
    rt = os.path.join(ROOT, 'exhume', 'runtime')
    for p in RUNTIME.values():
        if not os.path.exists(os.path.join(rt, *p)):
            sys.exit('rpbuild: exhume/runtime/%s is missing: git submodule update --init' % '/'.join(p))
    key = inputs_key(src, rt)
    stamp = os.path.join(out, 'build.sha1')
    if not a.force and os.path.exists(os.path.join(out, 'GAME.EXE')) and os.path.exists(stamp) \
            and open(stamp).read().strip() == key:
        print('rpbuild: %s is up to date (inputs %s)' % (os.path.join(out, 'GAME.EXE'), key[:12]))
        return 0
    tools = os.environ.get('U6_DOSTOOLS') or os.path.join(ROOT, 'tools')
    tc, md = os.path.join(tools, 'turboc20'), os.path.join(tools, 'masm300')
    base = None
    if os.name != 'nt':   # under the home folder, where a snap DOSBox-X can see it (as the gate)
        base = os.path.expanduser('~/tmp')
        os.makedirs(base, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='u6rp-', dir=base)
    try:
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        try:
            dosbox, note = gate.find_dosbox(tmp, env)
        except gate.Missing as m:
            sys.exit('rpbuild: %s' % m)
        print('rpbuild: DOSBox: %s' % note)
        c = os.path.join(tmp, 'c')
        os.makedirs(os.path.join(c, 'TOOLS'))
        shutil.copytree(tc, os.path.join(c, 'TOOLS', 'TURBOC20'))
        exe = [n for n in os.listdir(md) if n.upper() == 'MASM.EXE']
        img = [n for n in os.listdir(md) if n.lower().endswith('.img')]
        masm = open(os.path.join(md, exe[0]), 'rb').read() if exe else gate.fat12_extract(os.path.join(md, img[0]), 'MASM.EXE')
        for n in ('MASM300.EXE', 'TASM200.EXE'):   # the MAKEFILE names TASM; MASM 3.00 gives the same bytes
            open(os.path.join(c, 'TOOLS', n), 'wb').write(masm)
        s = os.path.join(c, 'S')
        shutil.copytree(src, s, ignore=shutil.ignore_patterns('*.OBJ', '*.obj', '*.EXE', '*.exe', '*.LIB'))
        for name, p in RUNTIME.items():
            open(os.path.join(s, name), 'wb').write(crlf(open(os.path.join(rt, *p), 'rb').read()))
        for n in CRLF_SRC:
            open(os.path.join(s, n), 'wb').write(crlf(open(os.path.join(src, n), 'rb').read()))
        os.makedirs(os.path.join(s, 'RP'))
        open(os.path.join(s, 'u6rp.lnk'), 'w', newline='').write(replay_lnk(src))
        open(os.path.join(c, 'BUILD.BAT'), 'w', newline='').write('\r\n'.join(batch_lines()) + '\r\n')
        conf = os.path.join(tmp, 'dosbox.conf')
        open(conf, 'w').write('[sdl]\noutput=surface\n[dosbox]\nmemsize=16\n[cpu]\ncycles=max\n'
                              '[autoexec]\nmount c "%s"\nc:\ncall \\BUILD.BAT\nexit\n' % c)
        args = [dosbox, '-conf', conf, '-nogui', '-exit'] if 'dosbox-x' in os.path.basename(dosbox).lower() \
            else [dosbox, '-conf', conf, '-noconsole']
        subprocess.run(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1800)

        def staged(n):
            for x in os.listdir(s):
                if x.upper() == n:
                    return os.path.join(s, x)
        log = open(os.path.join(c, 'BUILD.LOG'), encoding='latin-1').read() if os.path.exists(os.path.join(c, 'BUILD.LOG')) else ''
        lines = log.splitlines()
        for i, l in enumerate(lines):
            # Turbo C's and TLINK's errors; MASM's summary is "Warning Severe / Errors Errors / w s"
            masm = l.startswith('Errors\t') and i + 1 < len(lines) and lines[i + 1].split() != ['0', '0']
            if re.search(r'^(Error|Fatal)[ :]|Undefined symbol|^\*\*\*', l) or masm:
                print('log: ' + (l if not masm else 'MASM warnings, errors: ' + lines[i + 1]))
        bad = 0
        for n in ('GAME.EXE', 'GAMEREF.EXE'):
            p = staged(n)
            sha = hashlib.sha1(open(p, 'rb').read()).hexdigest() if p else None
            print('%-11s sha1 %s %s' % (n, sha, 'matches the gate' if sha == GAME_SHA1 else 'MISMATCH (expected %s)' % GAME_SHA1))
            bad += sha != GAME_SHA1
        rg = staged('RGAME.EXE')
        if not rg or not staged('RGAME.MAP') or not staged('GAMEREF.MAP'):
            print('rpbuild: no RGAME.EXE: the replay link failed (BUILD.LOG in %s)' % out)
            bad += 1
        else:
            bad += compare_maps(staged('GAMEREF.MAP'), staged('RGAME.MAP'))
        if os.path.exists(out):
            shutil.rmtree(out)
        os.makedirs(out)
        open(os.path.join(out, 'BUILD.LOG'), 'w', encoding='latin-1').write(log)
        for n in ('RGAME.MAP', 'GAMEREF.MAP', 'RGAME.EXE'):
            if staged(n):
                shutil.copy(staged(n), os.path.join(out, n))
        if a.keep_tree:
            shutil.copytree(s, os.path.join(out, 'tree'))
        if bad:
            print('rpbuild: FAIL: the DGROUP check or the normal build failed; no GAME.EXE in %s' % out)
            return 1
        shutil.copy(rg, os.path.join(out, 'GAME.EXE'))
        open(stamp, 'w').write(key + '\n')
        print('rpbuild: PASS: DGROUP unmoved; RGAME.EXE %d bytes sha1 %s -> %s' % (
            os.path.getsize(rg), hashlib.sha1(open(rg, 'rb').read()).hexdigest(), os.path.join(out, 'GAME.EXE')))
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
