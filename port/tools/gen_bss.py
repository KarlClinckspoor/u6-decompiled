#!/usr/bin/env python3
"""Generate port/sys/bss.c: the game's uninitialised globals (SRC/BSS.ASM) for the host.

    python3 port/tools/gen_bss.py

In DOS, BSS.ASM lays the globals out one after the other, and the game sometimes reads or
writes past one into the next (TextWindows, declared 9 bytes, is four 9-byte windows;
SpellPageNav[3] is SpellNextCircle). The port keeps that order and that contiguity: one
block of host memory, each global an assembler symbol set at its place in it. Objects that
hold pointers are bigger on the host (8-byte pointers), so their offsets differ from DOS's,
but every run of non-pointer objects stays exactly as contiguous as in DOS.

The sizes come from a probe compiled with the game's headers on the host (-m64) and with
32-bit pointers (-m32, the size of a DOS far pointer).
"""
import os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'SRC')


def parse_bss():
    lines = open(os.path.join(SRC, 'BSS.ASM'), encoding='latin-1').read().split('\n')
    i = next(k for k, l in enumerate(lines) if l.startswith('_BSS'))
    objs = []
    for l in lines[i + 1:]:
        code = l.split(';')[0].rstrip()
        if not code.strip() or code.startswith('_BSS') or code.strip() == 'END':
            continue
        m = re.match(r'^(\S*)\s+(db|dw|dd)\s+(.*)$', code)
        if not m:
            continue
        name, kind, rest = m.groups()
        unit = {'db': 1, 'dw': 2, 'dd': 4}[kind]
        rest = rest.strip()
        m2 = re.match(r'^(.*)\s+dup\s*\((.*)\)$', rest)
        if m2:
            cnt = eval(m2.group(1).replace('h', '').replace('0C00', '0xC00').replace('0Eh', '14'),
                       {}) if False else None
            expr = m2.group(1)
            expr = re.sub(r'\b([0-9][0-9A-Fa-f]*)h\b', lambda mm: str(int(mm.group(1), 16)), expr)
            cnt = eval(expr)
            per = len([x for x in m2.group(2).split(',')])
            size = cnt * unit * per
        else:
            size = unit * len(rest.split(','))
        if not name or name.startswith('__'):
            objs[-1][2] += size        # part of the previous object (struct parts, padding)
            objs[-1][3].append((name, size))
        else:
            objs.append([name, kind, size, []])
    return objs


def probe(names, bits):
    """for each name: sizeof the object (or 0 if incomplete) and sizeof its element"""
    src = ['#include "u6.h"', '#include "osi_file.h"', '#include <stdio.h>', 'int main(void){']
    for n in names:
        src.append('printf("%%s %%d %%d\\n", "%s", (int)sizeof(*&%s) * 0 + 0, 0);' % (n, n))
    return src


def main():
    objs = parse_bss()
    names = [o[0] for o in objs]
    # element sizes: write a probe using __typeof__ and arrays decaying
    with tempfile.TemporaryDirectory() as tmp:
        out = {}
        for bits in ('64', '32'):
            c = os.path.join(tmp, 'p.c')
            lines = ['#include "u6.h"', 'int printf(const char *, ...);', 'int main(void) {']
            for n in names:
                lines.append('printf("%s %%d %%d\\n", (int)sizeof(%s[0]), (int)__builtin_types_compatible_p(__typeof__(%s), __typeof__(&%s[0])));' % (n, n, n, n) if False else
                             'printf("%s %%d\\n", (int)sizeof(ELEM(%s)));' % (n, n))
            lines.append('return 0; }')
            pre = ['#define ELEM(x) (*IS_ARRAY_HELPER(x))']
            # an array's element: x[0]; a scalar/struct: the object itself
            body = '\n'.join(lines)
            open(c, 'w').write(body)
            # compile once per object kind decided below instead
        # simpler: classify from u6.h declarations
        decl = {}
        hdr = ''
        for h in ('u6.h', 'gamestat.h', 'mouse.h', 'osi_file.h', 'gr.h'):
            hdr += open(os.path.join(SRC, h), encoding='latin-1').read()
        for n in names:
            m = re.search(r'extern\s+([^;]*?\b%s\b[^;]*);' % re.escape(n), hdr)
            decl[n] = m.group(1) if m else None
        arr = {n: bool(decl[n] and re.search(r'\b%s\s*\[' % re.escape(n), decl[n])) for n in names}
        res = {}
        for bits in ('64', '32'):
            lines = ['#include "u6.h"']
            for n in names:
                e = '%s[0]' % n if arr[n] else n
                lines.append('int probe_%s = sizeof(%s);' % (n, e))
            c = os.path.join(tmp, 'p%s.c' % bits)
            open(c, 'w').write('\n'.join(lines) + '\n')
            obj = os.path.join(tmp, 'p%s.s' % bits)
            inc = os.path.join(ROOT, 'port', 'include')
            r = subprocess.run(['clang'] + (['--target=i686-w64-mingw32', '-isystem', '/usr/i686-w64-mingw32/include'] if bits == '32' else ['-m64']) + ['-std=gnu89', '-fsigned-char', '-w', '-I', SRC, '-I', inc,
                                '-include', os.path.join(inc, 'compat.h'), '-S', c, '-o', obj], capture_output=True, text=True)
            if r.returncode:
                sys.exit(r.stderr[-3000:])
            cur = None
            for l in open(obj).read().split('\n'):
                m = re.match(r'^_?probe_(\w+):', l)
                if m:
                    cur = m.group(1)
                    continue
                m = re.match(r'^\s*\.long\s+(\d+)', l)
                if m and cur:
                    res.setdefault(cur, {})[bits] = int(m.group(1))
                    cur = None
    entries = []
    off = 0
    for name, kind, dossize, parts in objs:
        e64, e32 = res[name]['64'], res[name]['32']
        if arr[name]:
            dos_elem = e32
            if kind == 'dw' and e32 == 4:      # near pointers: 2 bytes in DOS
                dos_elem = 2
            # element is a struct with far pointers: dos size per element = e32
            count = dossize // dos_elem
            host = count * e64
            extra = dossize - count * dos_elem
            host += extra
        else:
            if e64 == e32:
                host = dossize
            else:
                host = max(e64, dossize - e32 + e64)
        entries.append((name, off, host, dossize, decl[name]))
        off += host
    total = (off + 15) & ~15
    out = ['/* Generated by port/tools/gen_bss.py from SRC/BSS.ASM: do not edit.',
           ' * replaces BSS.ASM: the uninitialised globals, in DOS\'s order, in one block, so that',
           ' * the game\'s reads and writes past one global into the next land as in DOS. */',
           '#include <stdint.h>', '',
           '#define BSS_SIZE %d' % total,
           '__attribute__((aligned(16))) uint8_t u6_bss[BSS_SIZE];', '',
           '#if defined(__APPLE__)', '#define SYM(n) "_" #n', '#else', '#define SYM(n) #n', '#endif',
           '#define AT(n, off) __asm__(".globl " SYM(n) "\\n.set " SYM(n) ", " SYM(u6_bss) " + " #off);', '']
    for name, o, host, dossize, d in entries:
        out.append('AT(%s, %d) /* %d bytes (DOS %d): %s */' % (name, o, host, dossize, (d or '?').replace('*/', '* /')))
    out.append('')
    p = os.path.join(ROOT, 'port', 'sys', 'bss.c')
    open(p, 'w').write('\n'.join(out))
    print('%d objects, %d bytes' % (len(entries), total))


if __name__ == '__main__':
    main()
