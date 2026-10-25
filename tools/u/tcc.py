#!/usr/bin/env python3
"""Compile C files with Turbo C 2.0 (TCC.EXE) in a headless DOSBox.

    python3 tools/u/tcc.py [--flags "-mm -c -O"] [--out DIR] [--include DIR...] FILE.c ...

Each FILE.c is compiled on its own (in one DOSBox session); FILE.OBJ goes to --out
(default: next to the source) and the compiler's messages are printed. Used by the
U.EXE decompilation (tools/u/ucmp.py) to check one module at a time.
"""
import argparse, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
DEFAULT_FLAGS = '-mm -c'


def has_inline_asm(path):
    """a C file with asm statements (or #pragma inline) goes through the assembler"""
    if not path.lower().endswith('.c'):
        return False
    t = open(path, encoding='latin-1').read()
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    return bool(re.search(r'^\s*(asm\b|#\s*pragma\s+inline)', t, re.M))


def dosbox():
    return shutil.which('dosbox-x') or shutil.which('dosbox') or 'dosbox'


def compile_files(files, flags=DEFAULT_FLAGS, out=None, tools=None, includes=(), quiet=False, masm=False):
    """Compile (or with masm=True assemble with MASM 3.0); returns
    {src_path: (obj_path or None, messages)}."""
    tools = tools or os.environ.get('U6_DOSTOOLS') or next(
        (d for d in (os.path.join(ROOT, 'tools'), '/home/claude/dostools')
         if os.path.isdir(os.path.join(d, 'turboc20'))), os.path.join(ROOT, 'tools'))
    tc = os.path.join(tools, 'turboc20')
    res = {}
    with tempfile.TemporaryDirectory() as tmp:
        c = os.path.join(tmp, 'c')
        os.makedirs(os.path.join(c, 'TOOLS'))
        os.symlink(os.path.abspath(tc), os.path.join(c, 'TOOLS', 'TURBOC20')) if False else \
            shutil.copytree(tc, os.path.join(c, 'TOOLS', 'TURBOC20'))
        os.makedirs(os.path.join(c, 'S'))
        if masm:
            mdir = os.path.join(tools, 'masm300')
            exe = [n for n in os.listdir(mdir) if n.upper() == 'MASM.EXE']
            if exe:
                shutil.copy(os.path.join(mdir, exe[0]), os.path.join(c, 'TOOLS', 'MASM.EXE'))
            else:
                img = [n for n in os.listdir(mdir) if n.lower().endswith('.img')][0]
                subprocess.run(['mcopy', '-i', os.path.join(mdir, img), '::MASM.EXE',
                                os.path.join(c, 'TOOLS', 'MASM.EXE')], check=True)
        incs = []
        for k, d in enumerate(includes):
            dst = os.path.join(c, 'I%d' % k)
            shutil.copytree(d, dst, ignore=shutil.ignore_patterns('*.OBJ', '*.obj', '*.EXE', '*.exe', '*.c', '*.C'))
            for dp, _, fs in os.walk(dst):
                for fn in fs:
                    if fn.lower().endswith(('.h', '.inc')):
                        fp = os.path.join(dp, fn)
                        b = open(fp, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                        open(fp, 'wb').write(b)
            incs.append('\\I%d' % k)
        lines = ['@echo off', 'set PATH=\\TOOLS\\TURBOC20', 'cd \\S']
        names = []
        need_masm = masm or any(has_inline_asm(f) for f in files)
        if need_masm and not masm:
            mdir = os.path.join(tools, 'masm300')
            exe = [n for n in os.listdir(mdir) if n.upper() == 'MASM.EXE']
            if exe:
                shutil.copy(os.path.join(mdir, exe[0]), os.path.join(c, 'TOOLS', 'MASM.EXE'))
            else:
                img = [n for n in os.listdir(mdir) if n.lower().endswith('.img')][0]
                subprocess.run(['mcopy', '-o', '-i', os.path.join(mdir, img), '::MASM.EXE',
                                os.path.join(c, 'TOOLS', 'MASM.EXE')], check=True)
        for k, f in enumerate(files):
            base = 'F%03d' % k
            src = open(f, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
            names.append(base)
            if masm:
                # MASM 3.0 has no include path: the include files go next to the source
                for d in includes:
                    for fn in os.listdir(d):
                        if fn.lower().endswith(('.inc', '.h', '.asi')):
                            b = open(os.path.join(d, fn), 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                            open(os.path.join(c, 'S', fn.upper()), 'wb').write(b)
                open(os.path.join(c, 'S', base + '.ASM'), 'wb').write(src)
                lines.append('\\TOOLS\\MASM %s %s; > %s.LOG' % (flags, base, base))
                continue
            open(os.path.join(c, 'S', base + '.C'), 'wb').write(src)
            inc = ';'.join(['\\TOOLS\\TURBOC20\\INCLUDE'] + incs)
            if has_inline_asm(f):
                # Turbo C compiles a module with inline asm through the assembler (it ran
                # TASM; MASM 3.0 gives the same object): -S, then MASM
                lines.append('TCC -I%s %s -S %s.C > %s.LOG' % (inc, flags.replace('-c', '').replace('  ', ' '), base, base))
                lines.append('\\TOOLS\\MASM /Mx %s.ASM; >> %s.LOG' % (base, base))
            else:
                lines.append('TCC -I%s %s %s.C > %s.LOG' % (inc, flags, base, base))
        open(os.path.join(c, 'RUN.BAT'), 'w', newline='').write('\r\n'.join(lines) + '\r\n')
        conf = os.path.join(tmp, 'dosbox.conf')
        open(conf, 'w').write('[sdl]\noutput=surface\n[dosbox]\nmemsize=16\n[cpu]\ncycles=max\n'
                              '[autoexec]\nmount c "%s"\nc:\ncall \\RUN.BAT\nexit\n' % c)
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        db = dosbox()
        args = [db, '-conf', conf, '-noconsole'] if 'dosbox-x' not in os.path.basename(db).lower() \
            else [db, '-conf', conf, '-nogui', '-exit']
        subprocess.run(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
        for f, base in zip(files, names):
            log = ''
            lp = os.path.join(c, 'S', base + '.LOG')
            if os.path.exists(lp):
                log = open(lp, encoding='latin-1').read()
            obj = os.path.join(c, 'S', base + '.OBJ')
            dst = None
            if os.path.exists(obj):
                od = out or os.path.dirname(os.path.abspath(f))
                os.makedirs(od, exist_ok=True)
                dst = os.path.join(od, os.path.splitext(os.path.basename(f))[0] + '.OBJ')
                shutil.copy(obj, dst)
            msgs = [l for l in log.splitlines() if not l.startswith(('Warning Severe', 'Errors')) and (not masm and l.startswith(('Error', 'Warning', 'Fatal', '***'))) or
                    ((masm or has_inline_asm(f)) and (': error' in l.lower() or ': warning' in l.lower()))]
            if os.environ.get('TCC_LOG'):
                print(log)
            msgs = [m.replace(base + '.C', os.path.basename(f)) for m in msgs]
            res[f] = (dst, msgs)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--flags')
    ap.add_argument('--masm', action='store_true', help='assemble .ASM files with MASM 3.0 (default flags /Mx)')
    ap.add_argument('--out')
    ap.add_argument('--include', action='append', default=[])
    ap.add_argument('files', nargs='+')
    a = ap.parse_args()
    flags = a.flags if a.flags is not None else ('/Mx' if a.masm else DEFAULT_FLAGS)
    res = compile_files(a.files, flags, a.out, includes=a.include, masm=a.masm)
    bad = 0
    for f, (obj, msgs) in res.items():
        for m in msgs:
            print(m)
        print('%s -> %s' % (f, obj or 'FAILED'))
        bad += obj is None
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
