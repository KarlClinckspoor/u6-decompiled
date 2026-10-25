#!/usr/bin/env python3
"""Check every finished module of SRC_U against U.EXE (tools/u/ucmp.py), in parallel.

    python3 tools/u/ucheck.py [MODULE ...]
"""
import os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

# file, frame, --at, code length, _DATA range
MODULES = [
    ('main.c', '00E1', '6', '181A', ('0194', '03CA')),
    ('transfer.c', '0263', '0', '1A36', ('03CA', '0692')),
    ('chargen.c', '0406', '6', '2700', ('0692', '0C90')),
    ('credits.c', '0676', '6', '738', ('0C90', '0FE4')),
    ('intro.c', '06E9', 'E', '2B6D', ('0FE4', '13E2')),
    ('passage.c', '09A0', 'B', '2205', ('13E2', '4038')),
    ('disk.c', '0BC1', '0', '560', ('4038', '40C4')),
    ('moongate.c', '0C17', '0', '2FE6', ('40C4', '410E')),
    ('criterr.asm', '0F15', '6', '38', ('410E', '4110')),
    ('lzw.asm', '0F18', 'E', '365', ('4110', '413E')),
]


def check(m):
    f, seg, at, ln, (d0, d1) = m
    r = subprocess.run([sys.executable, os.path.join(HERE, 'ucmp.py'), os.path.join(ROOT, 'SRC_U', f), '--seg', seg,
                        '--at', at, '--length', ln, '--data-range', d0, d1, '--include', os.path.join(ROOT, 'SRC_U')],
                       capture_output=True, text=True)
    lines = [l for l in r.stdout.splitlines() if not l.startswith('Warning ')]
    return f, r.returncode == 0, lines


def main():
    want = sys.argv[1:]
    mods = [m for m in MODULES if not want or m[0] in want or m[0].split('.')[0] in want]
    bad = 0
    with ThreadPoolExecutor(max_workers=6) as ex:
        for f, ok, lines in ex.map(check, mods):
            print('%-12s %s' % (f, 'OK' if ok else 'DIFFERENT'))
            if not ok:
                bad += 1
                for l in lines[:25]:
                    print('    ' + l)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
