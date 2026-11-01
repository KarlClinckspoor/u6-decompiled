#!/usr/bin/env python3
"""Give every integer type in the shared sources an explicit width.

    python3 tools/widths.py SRC

Turbo C's int is 16 bits and its long 32; a 64-bit host's int is 32 bits and its long
64 (32 on Windows). Each run of the type keywords signed/unsigned/short/int/long (not
with char) in a type is replaced by portable.h's int16, uint16, int32 or uint32, which are
those same types under Turbo C, so the DOS build compiles to the same bytes (proved by
tools/dosbuild.py). Locals and parameters are converted too: a 16-bit local truncates its
stores exactly as DOS did. Keywords in comments, strings and preprocessor lines other
than #define bodies are left alone.
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctok

KW = {'signed', 'unsigned', 'short', 'int', 'long'}
SKIP_FILES = {'portable.h'}


def width(words):
    w = set(words)
    if 'char' in w:
        return None
    uns = 'unsigned' in w
    if 'long' in w:
        return 'uint32' if uns else 'int32'
    return 'uint16' if uns else 'int16'


def rewrite(src):
    toks = ctok.tokenize(src)
    out = []
    i = 0
    n = len(toks)
    changed = 0

    def sig(j):
        while j < n and toks[j][0] in ('ws', 'nl', 'comment'):
            j += 1
        return j

    while i < n:
        k, t = toks[i][0], toks[i][1]
        if k == 'pp':
            body, c = rewrite_pp(t)
            out.append(body)
            changed += c
            i += 1
            continue
        if k == 'id' and t in KW:
            # gather the run (keywords separated only by whitespace)
            words = [t]
            j = i + 1
            last = i
            while True:
                m = sig(j)
                if m < n and toks[m][0] == 'id' and toks[m][1] in KW | {'char'}:
                    words.append(toks[m][1])
                    last = m
                    j = m + 1
                else:
                    break
            w = width(words)
            if w is None:
                out.append(''.join(x[1] for x in toks[i:last + 1]))
            else:
                out.append(w)
                changed += 1
            i = last + 1
            continue
        out.append(t)
        i += 1
    return ''.join(out), changed


def rewrite_pp(text):
    m = re.match(r'(\s*#\s*define\b)(.*)$', text, re.S)
    if not m:
        return text, 0
    body, c = rewrite(m.group(2))
    return m.group(1) + body, c


def main():
    root = sys.argv[1]
    total = 0
    for name in sorted(os.listdir(root)):
        if not name.lower().endswith(('.c', '.h')) or name in SKIP_FILES:
            continue
        p = os.path.join(root, name)
        s = open(p, encoding='latin-1', newline='').read()
        new, c = rewrite(s)
        if c:
            open(p, 'w', encoding='latin-1', newline='').write(new)
        print('%-12s %d' % (name, c))
        total += c
    print('total', total)


if __name__ == '__main__':
    main()
