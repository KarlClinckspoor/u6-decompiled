#!/usr/bin/env python3
"""Build GAME.EXE with the original tools in a headless DOSBox, and check it.

    python3 tools/dosbuild.py [--tools DIR] [--dosbox PATH] [--keep DIR] [--check SHA1]

The DOS build is the reference for the port: every change to SRC/ must still give the
same GAME.EXE. This copies SRC/ to a scratch C: drive together with Turbo C 2.0
(`tools/turboc20`) and MASM 3.0 (`tools/masm300`, the MASM.EXE file or the floppy image
it came on), runs OSILIB's assembly and the MAKEFILE in DOSBox (or DOSBox-X), and prints
the SHA-1 of the linked GAME.EXE (before EXEPACK). With --check, it fails unless the
SHA-1 is the one given; `tools/game.sha1` holds the expected value and is used by
default when it exists.

TASM 2.0 is not needed: the three files the MAKEFILE gives TASM assemble with MASM 3.0
to the same objects (both build trees go through the same assembler).
"""
import argparse, hashlib, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

OSILIB_STEPS = [
    ('OSI_FILE', 'TASM'), ('KBD', 'MASM'), ('INFLATE', 'MASM'), ('MOUSE', 'TASM'), ('LOW', 'MASM'),
    ('DELAY', 'MASM'), ('DELAY2', 'MASM'), ('RAND', 'MASM'), ('SOUND1', 'MASM'), ('SOUND2', 'MASM'),
    ('SOUND3', 'MASM'), ('SOUND4', 'MASM'), ('SOUND5', 'MASM'),
]


def find_masm(masm_dir):
    for name in os.listdir(masm_dir):
        if name.upper() == 'MASM.EXE':
            return os.path.join(masm_dir, name), None
    for name in os.listdir(masm_dir):
        if name.lower().endswith('.img'):
            return None, os.path.join(masm_dir, name)
    sys.exit('no MASM.EXE or floppy image in %s' % masm_dir)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=os.path.join(ROOT, 'SRC'))
    ap.add_argument('--tools', default=os.path.join(ROOT, 'tools'))
    ap.add_argument('--dosbox', default=shutil.which('dosbox-x') or shutil.which('dosbox') or 'dosbox')
    ap.add_argument('--keep', help='copy the build directory here')
    ap.add_argument('--check', help='expected SHA-1 (default: tools/game.sha1)')
    ap.add_argument('--log', action='store_true', help='print the build log')
    a = ap.parse_args()

    tc = os.path.join(a.tools, 'turboc20')
    masm, img = find_masm(os.path.join(a.tools, 'masm300'))
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, 'c')
        os.makedirs(os.path.join(c, 'TOOLS'))
        shutil.copytree(tc, os.path.join(c, 'TOOLS', 'TURBOC20'))
        dst = os.path.join(c, 'TOOLS', 'MASM300.EXE')
        if masm:
            shutil.copy(masm, dst)
        else:
            subprocess.run(['mcopy', '-i', img, '::MASM.EXE', dst], check=True)
        shutil.copy(dst, os.path.join(c, 'TOOLS', 'TASM200.EXE'))
        shutil.copytree(a.src, os.path.join(c, 'S'), ignore=shutil.ignore_patterns('*.OBJ', '*.obj', '*.EXE', '*.exe', '*.LIB'))
        lines = ['@echo off', 'set PATH=\\TOOLS\\TURBOC20', 'cd \\S\\OSILIB']
        for name, asm in OSILIB_STEPS:
            tool = 'TASM200' if asm == 'TASM' else 'MASM300'
            lines.append('\\TOOLS\\%s /Mx %s; >> \\BUILD.LOG' % (tool, name))
        lines.append('TLIB OSISND.LIB +SOUND1 +SOUND2 +SOUND3 +SOUND4 +SOUND5 >> \\BUILD.LOG')
        lines.append('cd \\S')
        lines.append('make game.exe >> \\BUILD.LOG')
        open(os.path.join(c, 'BUILD.BAT'), 'w', newline='').write('\r\n'.join(lines) + '\r\n')
        conf = os.path.join(tmp, 'dosbox.conf')
        open(conf, 'w').write('[sdl]\noutput=surface\n[dosbox]\nmemsize=16\n[cpu]\ncycles=max\n'
                              '[autoexec]\nmount c "%s"\nc:\ncall \\BUILD.BAT\nexit\n' % c)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        subprocess.run([a.dosbox, '-conf', conf, '-noconsole'] if 'dosbox-x' not in os.path.basename(a.dosbox).lower()
                       else [a.dosbox, '-conf', conf, '-nogui', '-exit'],
                       env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1800)
        log = open(os.path.join(c, 'BUILD.LOG'), encoding='latin-1').read() if os.path.exists(os.path.join(c, 'BUILD.LOG')) else ''
        if a.log:
            print(log)
        exe = None
        for name in os.listdir(os.path.join(c, 'S')):
            if name.upper() == 'GAME.EXE':
                exe = os.path.join(c, 'S', name)
        errors = [l for l in log.splitlines() if l.startswith(('Error ', 'Fatal ')) or 'Undefined symbol' in l]
        for l in errors:
            print(l)
        if a.keep:
            shutil.copytree(os.path.join(c, 'S'), a.keep, dirs_exist_ok=True)
        if not exe:
            sys.exit('no GAME.EXE was built')
        sha = hashlib.sha1(open(exe, 'rb').read()).hexdigest()
        print('GAME.EXE %d bytes sha1 %s' % (os.path.getsize(exe), sha))
        want = a.check
        if not want and os.path.exists(os.path.join(HERE, 'game.sha1')):
            want = open(os.path.join(HERE, 'game.sha1')).read().split()[0]
        if want:
            if sha != want:
                sys.exit('MISMATCH: expected %s' % want)
            print('identical to the reference build')


if __name__ == '__main__':
    main()
