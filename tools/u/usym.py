#!/usr/bin/env python3
"""The symbol table of the U.EXE decompilation, and the pass that links the modules by
name: references by address (C_SSSS_OOOO, D_XXXX) become the names the defining module
gave them.

    python3 tools/u/usym.py list            the table (address, name, defined in)
    python3 tools/u/usym.py apply           rewrite SRC_U (C and assembly)

Where the names come from:
  - C:        /*C_SSSS_OOOO*/ before a function definition; /*XXXX*/ before a variable.
  - assembly: `FProc NAME ;SSSS:OOOO`, `NAME PROC ... ;SSSS:OOOO`, and data
              `NAME dw ... ;D_XXXX`.
A C function's link name is _name; assembly called from C gets a leading underscore so
that C can call it by the same name (pascal routines keep their names: Turbo C upper-
cases them and TLINK ignores case).
"""
import collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'SRC_U')
PASCAL = {(0x0000, 0x09F2): 'OSI_callback', (0x0000, 0x09F3): 'OSI_open', (0x0000, 0x0A32): 'OSI_close',
          (0x0000, 0x0A66): 'OSI_create', (0x0000, 0x0AA6): 'OSI_delete', (0x0000, 0x0AE4): 'OSI_read',
          (0x0000, 0x0BCA): 'OSI_write', (0x0000, 0x0C8F): 'OSI_rename', (0x0000, 0x0CD4): 'OSI_copy',
          (0x0000, 0x0DCA): 'OSI_fileSize', (0x0000, 0x0DE7): 'OSI_memory'}


def files():
    out = []
    for d in (SRC, os.path.join(SRC, 'OSILIB')):
        for fn in sorted(os.listdir(d)):
            if fn.lower().endswith(('.c', '.h', '.asm', '.inc')):
                out.append(os.path.join(d, fn))
    return out


def read(p):
    return open(p, encoding='latin-1').read()


def table():
    """{('C', seg, off) or ('D', off): (name, file, lang)}"""
    t = {}
    for p in files():
        s = read(p)
        fn = os.path.relpath(p, SRC)
        if p.lower().endswith(('.c', '.h')):
            for m in re.finditer(r'/\*C_([0-9A-F]{4})_([0-9A-F]{4})\*/\s*((?:[\w\*]+\s+)*?)\**\s*(\w+)\s*\(', s):
                quals = m.group(3)
                t[('C', int(m.group(1), 16), int(m.group(2), 16))] = (m.group(4), fn, 'c', 'static' in quals)
            for m in re.finditer(r'/\*([0-9A-F]{4})\*/\s*(?:static\s+)?[\w\s\*]+?\b(D_[0-9A-F]{4}|\w+)\s*[\[=;]', s):
                t.setdefault(('D', int(m.group(1), 16)), (m.group(2), fn, 'c', False))
        else:
            for m in re.finditer(r'^FProc\s+(\w+)\s*;\s*([0-9A-F]{4}):([0-9A-F]{4})', s, re.M):
                t[('C', int(m.group(2), 16), int(m.group(3), 16))] = (m.group(1), fn, 'asm', False)
            for m in re.finditer(r'^(\w+)\s+PROC\s+\w+\s*;\s*([0-9A-F]{4}):([0-9A-F]{4})', s, re.M):
                t[('C', int(m.group(2), 16), int(m.group(3), 16))] = (m.group(1), fn, 'asm', False)
            for m in re.finditer(r'^(\w+)\s+(?:db|dw|dd|LABEL)\b[^;\n]*;\s*D_([0-9A-F]{4})\b', s, re.M):
                t[('D', int(m.group(2), 16))] = (m.group(1), fn, 'asm', False)
    for (sg, o), n in PASCAL.items():
        t[('C', sg, o)] = (n, 'OSILIB/OSI_FILE.ASM', 'pascal', False)
    return t


def cname(entry):
    """the name C code uses for a symbol"""
    name, fn, lang, static = entry
    if lang == 'asm':
        return name[1:] if name.startswith('_') else name
    return name


def main():
    t = table()
    if len(sys.argv) < 2 or sys.argv[1] == 'list':
        byname = collections.defaultdict(list)
        for k, v in sorted(t.items()):
            addr = 'C_%04X_%04X' % k[1:] if k[0] == 'C' else 'D_%04X' % k[1]
            print('%-12s %-28s %s%s' % (addr, v[0], v[1], ' (static)' if v[3] else ''))
            if not v[3]:
                byname[cname(v).lower()].append(addr)
        for n, addrs in byname.items():
            if len(addrs) > 1:
                print('DUPLICATE public name %s: %s' % (n, ' '.join(addrs)))
        return
    if sys.argv[1] == 'apply':
        cmap, amap = {}, {}  # address name -> name for C code / for assembly
        rename_asm_defs = {}  # file -> {asm label: new label (gets an underscore)}
        used_by_c = set()
        for p in files():
            if p.lower().endswith(('.c', '.h')):
                used_by_c.update(re.findall(r'\b(C_[0-9A-F]{4}_[0-9A-F]{4}|D_[0-9A-F]{4})\b', read(p)))
        for k, v in t.items():
            addr = 'C_%04X_%04X' % k[1:] if k[0] == 'C' else 'D_%04X' % k[1]
            if v[2] == 'c' and v[0] == addr:
                continue  # still named by its address
            cmap[addr] = cname(v)
            if v[2] == 'asm':
                label = v[0]
                if addr in used_by_c and not label.startswith('_'):
                    label = '_' + v[0]
                    rename_asm_defs.setdefault(v[1], {})[v[0]] = label
                cmap[addr] = label[1:] if label.startswith('_') else label
                amap[addr] = label
            elif v[2] == 'pascal':
                amap[addr] = v[0]
            else:
                amap[addr] = '_' + v[0]
        tok = re.compile(r'\b(C_[0-9A-F]{4}_[0-9A-F]{4}|D_[0-9A-F]{4})\b')
        for p in files():
            s = read(p)
            o = s
            if p.lower().endswith(('.c', '.h')):
                # not the address comments /*C_SSSS_OOOO*/ themselves
                s = re.sub(r'(/\*)?\b(C_[0-9A-F]{4}_[0-9A-F]{4}|D_[0-9A-F]{4})\b(\*/)?',
                           lambda m: m.group(0) if (m.group(1) and m.group(3)) else
                           (m.group(1) or '') + cmap.get(m.group(2), m.group(2)) + (m.group(3) or ''), s)
            else:
                out = []
                for line in s.split('\n'):
                    code, sep, com = line.partition(';')
                    code = tok.sub(lambda m: amap.get(m.group(1), m.group(1)), code)
                    for old, new in rename_asm_defs.get(os.path.relpath(p, SRC), {}).items():
                        code = re.sub(r'(?<!\w)%s\b' % re.escape(old), new, code)
                    out.append(code + sep + com)
                s = '\n'.join(out)
            if s != o:
                open(p, 'w', encoding='latin-1').write(s)
                print('rewrote', os.path.relpath(p, SRC))


if __name__ == '__main__':
    main()
