#!/usr/bin/env python3
"""Build U.EXE from SRC_U/ with the original tools in a headless DOSBox, and compare it
with the original.

    python3 tools/u/ubuild.py [--keep DIR] [--log]

SRC_U/u.lnk lists the objects in link order (after C0M); SRC_U/OSILIB/osilib.lst lists
the library modules (their sources in SRC_U/OSILIB) in the order they are put into
OSILIB.LIB, which is linked after Turbo C's CM.LIB, as the original was. C files are
compiled with `tcc -mm -c -O -G`, assembly with MASM 3.0 /Mx.

The result is compared with U.EXE byte for byte (header, relocations, image); the first
differences are shown with the module they fall in.
"""
import argparse, hashlib, os, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tcc  # noqa: E402
from ucmp import find_uexe  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'SRC_U')
FLAGS = '-mm -c -O -G'
LIBFLAGS = '-mm -c -G'   # the library's C was compiled without -O


def crlf(path, dst):
    b = open(path, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    open(dst, 'wb').write(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--keep')
    ap.add_argument('--log', action='store_true')
    a = ap.parse_args()
    tools = os.environ.get('U6_DOSTOOLS') or next(
        (d for d in (os.path.join(ROOT, 'tools'), '/home/claude/dostools')
         if os.path.isdir(os.path.join(d, 'turboc20'))))
    objs = open(os.path.join(SRC, 'u.lnk')).read().split()
    libmods = open(os.path.join(SRC, 'OSILIB', 'osilib.lst')).read().split()
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, 'c')
        os.makedirs(os.path.join(c, 'TOOLS'))
        shutil.copytree(os.path.join(tools, 'turboc20'), os.path.join(c, 'TOOLS', 'TURBOC20'))
        mdir = os.path.join(tools, 'masm300')
        exe = [n for n in os.listdir(mdir) if n.upper() == 'MASM.EXE']
        if exe:
            shutil.copy(os.path.join(mdir, exe[0]), os.path.join(c, 'TOOLS', 'MASM.EXE'))
        else:
            img = [n for n in os.listdir(mdir) if n.lower().endswith('.img')][0]
            subprocess.run(['mcopy', '-i', os.path.join(mdir, img), '::MASM.EXE',
                            os.path.join(c, 'TOOLS', 'MASM.EXE')], check=True)
        s = os.path.join(c, 'S')
        os.makedirs(os.path.join(s, 'L'))
        for fn in os.listdir(SRC):
            if fn.lower().endswith(('.c', '.h', '.asm', '.inc')):
                crlf(os.path.join(SRC, fn), os.path.join(s, fn.upper()))
        for fn in os.listdir(os.path.join(SRC, 'OSILIB')):
            if fn.lower().endswith(('.c', '.h', '.asm', '.inc')):
                crlf(os.path.join(SRC, 'OSILIB', fn), os.path.join(s, 'L', fn.upper()))
        for fn in os.listdir(SRC):  # headers the library's C may include
            if fn.lower().endswith('.h'):
                crlf(os.path.join(SRC, fn), os.path.join(s, 'L', fn.upper()))
        lines = ['@echo off', 'set PATH=\\TOOLS\\TURBOC20', 'cd \\S\\L']
        for m in libmods:
            base = m.upper()
            if os.path.exists(os.path.join(SRC, 'OSILIB', m + '.c')) or os.path.exists(os.path.join(SRC, 'OSILIB', m + '.C')):
                lines.append('TCC -I\\TOOLS\\TURBOC20\\INCLUDE %s %s.C >> \\BUILD.LOG' % (LIBFLAGS, base))
            else:
                lines.append('\\TOOLS\\MASM /Mx %s; >> \\BUILD.LOG' % base)
        lines.append('TLIB OSILIB.LIB %s >> \\BUILD.LOG' % ' '.join('+' + m.upper() for m in libmods))
        lines.append('cd \\S')
        for o in objs:
            base = o.upper()
            if os.path.exists(os.path.join(SRC, o + '.c')) and tcc.has_inline_asm(os.path.join(SRC, o + '.c')):
                lines.append('TCC -I\\TOOLS\\TURBOC20\\INCLUDE %s -S %s.C >> \\BUILD.LOG' % (FLAGS.replace('-c ', ''), base))
                lines.append('\\TOOLS\\MASM /Mx %s.ASM; >> \\BUILD.LOG' % base)
            elif os.path.exists(os.path.join(SRC, o + '.c')):
                lines.append('TCC -I\\TOOLS\\TURBOC20\\INCLUDE %s %s.C >> \\BUILD.LOG' % (FLAGS, base))
            else:
                lines.append('\\TOOLS\\MASM /Mx %s; >> \\BUILD.LOG' % base)
        open(os.path.join(s, 'U.RSP'), 'w', newline='').write(
            '\\TOOLS\\TURBOC20\\LIB\\C0M ' + ' '.join(o.upper() for o in objs) +
            ',U.EXE,U.MAP,\\TOOLS\\TURBOC20\\LIB\\CM L\\OSILIB\r\n')
        lines.append('TLINK /m @U.RSP >> \\BUILD.LOG')
        open(os.path.join(c, 'BUILD.BAT'), 'w', newline='').write('\r\n'.join(lines) + '\r\n')
        conf = os.path.join(tmp, 'dosbox.conf')
        open(conf, 'w').write('[sdl]\noutput=surface\n[dosbox]\nmemsize=16\n[cpu]\ncycles=max\n'
                              '[autoexec]\nmount c "%s"\nc:\ncall \\BUILD.BAT\nexit\n' % c)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        db = tcc.dosbox()
        args = [db, '-conf', conf, '-noconsole'] if 'dosbox-x' not in os.path.basename(db).lower() \
            else [db, '-conf', conf, '-nogui', '-exit']
        subprocess.run(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1800)
        log = open(os.path.join(c, 'BUILD.LOG'), encoding='latin-1').read() if os.path.exists(os.path.join(c, 'BUILD.LOG')) else ''
        if a.log:
            print(log)
        for l in log.splitlines():
            if l.startswith(('Error', 'Fatal', 'Warning:', 'Undefined')) or 'Undefined symbol' in l or 'rror' in l:
                print(l)
        if a.keep:
            shutil.copytree(s, a.keep, dirs_exist_ok=True)
        built = os.path.join(s, 'U.EXE')
        if not os.path.exists(built):
            sys.exit('no U.EXE was built')
        new = open(built, 'rb').read()
        mapf = open(os.path.join(s, 'U.MAP'), encoding='latin-1').read() if os.path.exists(os.path.join(s, 'U.MAP')) else ''
    old = open(find_uexe(), 'rb').read()
    print('U.EXE %d bytes sha1 %s; original %d bytes sha1 %s' % (
        len(new), hashlib.sha1(new).hexdigest(), len(old), hashlib.sha1(old).hexdigest()))
    if new == old:
        print('identical to the original')
        return
    hn, ho = struct.unpack_from('<H', new, 8)[0] * 16, struct.unpack_from('<H', old, 8)[0] * 16
    if new[:hn] != old[:ho]:
        print('headers/relocations differ (header %x vs %x bytes)' % (hn, ho))
    ni, oi = new[hn:], old[ho:]
    diffs = [i for i in range(min(len(ni), len(oi))) if ni[i] != oi[i]]
    print('image: %d vs %d bytes, %d bytes differ' % (len(ni), len(oi), len(diffs)))
    shown = 0
    last = -100
    for i in diffs:
        if i - last > 16 and shown < 12:
            print('  at image %05X: %s | original %s' % (i, ni[i:i + 8].hex(), oi[i:i + 8].hex()))
            shown += 1
        last = i
    if mapf and a.log:
        print(mapf)
    sys.exit(1)


if __name__ == '__main__':
    main()
