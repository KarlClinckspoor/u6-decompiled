#!/usr/bin/env python3
"""Compare a Turbo C object module with its part of the original U.EXE.

    python3 tools/u/ucmp.py MODULE.OBJ --seg 09A0 [--data 1234] [--learn] [-v]
    python3 tools/u/ucmp.py MODULE.c --seg 09A0 ...      (compiles it first, tools/u/tcc.py)

The module's code segment (the one of class CODE) is compared byte for byte with the
original image at paragraph --seg. Bytes under fixups are not compared directly; each
fixup is checked against what the original holds there instead:
  - a far call/pointer to an external name: the original's seg:off must be that name's
    address in tools/u/usyms.txt (with --learn, unknown names are added to it);
  - a far pointer into the module's own code segment: seg must be --seg, off the target;
  - an offset into _DATA/_BSS/DGROUP: the original's DS offset must be the segment's base
    plus the displacement; the base comes from --data/--bss or from the first such fixup,
    and every other one must agree with it;
  - an offset of an external variable: its DS offset in usyms.txt (learned likewise);
  - a segment base: DGROUP is 1499, the module's own segment is --seg.
The module's initialised data (_DATA) is compared with the original data segment at the
base found (or given), fixups inside it checked the same way.

Exit status 0 when everything matches. On a difference, prints where (segment offset and
the function containing it) with both disassemblies around it.
"""
import argparse, os, re, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import omf  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
DGROUP = 0x1499
SYMS = os.path.join(HERE, 'usyms.txt')


def find_uexe():
    for p in (os.environ.get('U6_UEXE'), os.path.join(ROOT, 'gamedata', 'U.EXE'),
              os.path.join(ROOT, '..', 'gamedata', 'U.EXE'), '/home/claude/gamedata/U.EXE'):
        if p and os.path.exists(p):
            return p
    sys.exit('U.EXE not found: set U6_UEXE')


def load_image(path):
    d = open(path, 'rb').read()
    hdrp = struct.unpack_from('<H', d, 8)[0]
    nrel, relofs = struct.unpack_from('<H', d, 6)[0], struct.unpack_from('<H', d, 0x18)[0]
    relocs = set()
    for i in range(nrel):
        off, seg = struct.unpack_from('<HH', d, relofs + 4 * i)
        relocs.add(seg * 16 + off)
    return d[hdrp * 16:], relocs


def load_syms():
    code, data = {}, {}
    if os.path.exists(SYMS):
        for line in open(SYMS):
            line = line.split('#')[0].split()
            if len(line) != 2:
                continue
            name, addr = line
            if ':' in addr:
                s, o = addr.split(':')
                code[name] = (int(s, 16), int(o, 16))
            else:
                data[name] = int(addr, 16)
    return code, data


class AddrNames(dict):
    """usyms.txt, plus the names that carry their own address: C_SSSS_OOOO (code, as
    _C_... for C, C_... for pascal) and D_XXXX (a DS offset)"""
    def __init__(self, base, pat, conv):
        dict.__init__(self, base)
        self.pat, self.conv = re.compile(pat, re.I), conv

    def get(self, name, default=None):
        if dict.__contains__(self, name):
            return dict.__getitem__(self, name)
        mm = self.pat.fullmatch(name)
        return self.conv(mm) if mm else default


def save_learned(new):
    if not new:
        return
    with open(SYMS, 'a') as f:
        for name, addr in new:
            f.write('%s %s\n' % (name, addr))


def switch_tables(blob, shift=0):
    """(offset, length) of Turbo C switch jump tables: jmp [cs:bx+T] after a cmp/ja"""
    out = []
    for mm in re.finditer(rb'\x2e\xff\xa7(..)', blob, re.S):
        t = (struct.unpack('<H', mm.group(1))[0] - shift) & 0xFFFF
        before = blob[max(0, mm.start() - 24):mm.start()]
        c = list(re.finditer(rb'\x3d(..)[\x77\x76]|\x83\xf8(.)[\x77\x76]', before, re.S))
        if not c:
            continue
        g = c[-1]
        n = (struct.unpack('<H', g.group(1))[0] if g.group(1) else g.group(2)[0]) + 1
        if n <= 256 and t + 2 * n <= len(blob):
            out.append((t, 2 * n))
    return out


def disasm(blob, origin, start, end, skips=()):
    with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as t:
        t.write(blob)
    try:
        args = ['ndisasm', '-b16', '-o', str(origin)]
        for o, n in skips:
            args += ['-k', '%d,%d' % (o, n)]
        out = subprocess.run(args + [t.name], capture_output=True, text=True).stdout
    finally:
        os.unlink(t.name)
    lines = []
    for l in out.splitlines():
        a = int(l.split()[0], 16)
        if start <= a < end and 'skipping' not in l:
            lines.append(l)
    return lines


def func_starts(code, length):
    """offsets of 'push bp; mov bp,sp' (55 8B EC) in a segment: function starts"""
    return [m.start() for m in re.finditer(b'\x55\x8b\xec', bytes(code[:length]))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('obj')
    ap.add_argument('--seg', required=True, help='paragraph of the module code segment in the image (hex)')
    ap.add_argument('--at', default='0', help='offset of the module code in that segment (hex): for _TEXT pieces')
    ap.add_argument('--data', help='DS offset of the module _DATA (hex)')
    ap.add_argument('--bss', help='DS offset of the module _BSS (hex)')
    ap.add_argument('--data-range', nargs=2, metavar=('START', 'END'),
                    help='the module _DATA must be exactly DS START..END (hex; END exclusive, a one-byte '
                         'alignment gap before END allowed)')
    ap.add_argument('--flags', default='-mm -c -O -G', help='tcc flags when given a .c file (MASM: /Mx)')
    ap.add_argument('--include', action='append', default=[])
    ap.add_argument('--learn', action='store_true', help='add unknown external names to usyms.txt')
    ap.add_argument('--length', help='expected code length (hex): also reports a short or long module')
    ap.add_argument('-v', action='store_true')
    a = ap.parse_args()

    path = a.obj
    if path.lower().endswith(('.c', '.asm')):
        import tcc
        masm = path.lower().endswith('.asm')
        flags = '/Mx' if masm and a.flags == '-mm -c -O -G' else a.flags
        res = tcc.compile_files([path], flags, out=tempfile.mkdtemp(), includes=a.include, masm=masm)
        obj, msgs = res[path]
        for m in msgs:
            print(m)
        if not obj:
            sys.exit('compile failed')
        path = obj
    img, relocs = load_image(find_uexe())
    syms, dsyms = load_syms()
    addr2name = {v: k for k, v in syms.items()}
    syms = AddrNames(syms, r'_?C_([0-9A-F]{4})_([0-9A-F]{4})', lambda mm: (int(mm.group(1), 16), int(mm.group(2), 16)))
    dsyms = AddrNames(dsyms, r'_?D_([0-9A-F]{4})', lambda mm: int(mm.group(1), 16))
    m = omf.parse_obj(path)
    cseg = [s for s in m.segs[1:] if s.cls == 'CODE']
    if len(cseg) != 1:
        sys.exit('expected one CODE segment, got %s' % cseg)
    cs = cseg[0]
    segp = int(a.seg, 16)
    at = int(a.at, 16)
    base = segp * 16 + at
    bases = {}
    if a.data:
        bases['_DATA'] = int(a.data, 16)
    if a.data_range:
        bases['_DATA'] = int(a.data_range[0], 16)
    if a.bss:
        bases['_BSS'] = int(a.bss, 16)
    learned = []
    problems = []

    def ds_word(addr):
        return struct.unpack_from('<H', img, addr)[0]

    def vote_bases(seg, at):
        from collections import Counter
        votes = {}
        for f in seg.fixups:
            if f.loc in (1, 5) and f.tkind in ('seg', 'grp') and f.target in ('_DATA', '_BSS', 'DGROUP'):
                tgt = '_DATA' if f.tkind == 'grp' else f.target
                inplace = struct.unpack_from('<H', seg.data, f.offset)[0] if f.offset + 2 <= len(seg.data) else 0
                v = struct.unpack_from('<H', img, at + f.offset)[0]
                votes.setdefault(tgt, Counter())[(v - f.disp - inplace) & 0xFFFF] += 1
        for tgt, c in votes.items():
            if tgt not in bases:
                bases[tgt] = c.most_common(1)[0][0]

    def check_fixups(seg, seg_addr):
        """seg_addr: image address of seg's byte 0"""
        for f in seg.fixups:
            addr = seg_addr + f.offset
            where = (seg.name, f.offset)
            # Turbo C keeps the displacement in the record's data, not in the fixup
            if f.loc in (1, 2, 5, 3) and f.offset + 2 <= len(seg.data) and f.loc != 2:
                f.disp = (f.disp + struct.unpack_from('<H', seg.data, f.offset)[0]) & 0xFFFF
            if f.loc == 3:  # far pointer
                off, sg = struct.unpack_from('<HH', img, addr)
                if f.tkind == 'ext':
                    want = syms.get(f.target)
                    have = addr2name.get((sg, off)) or 'C_%04X_%04X' % (sg, off)
                    if want is None:
                        known = addr2name.get((sg, off))
                        if known and known != f.target:
                            problems.append((where, 'far %s: original calls %s (%04x:%04x)' % (f.target, have, sg, off)))
                        else:
                            learned.append((f.target, '%04X:%04X' % (sg, off)))
                            syms[f.target] = (sg, off)
                            addr2name[(sg, off)] = f.target
                    elif want != (sg, off):
                        problems.append((where, 'far %s: original has %04x:%04x %s, %s is at %04x:%04x' % (
                            f.target, sg, off, have or '', f.target, want[0], want[1])))
                elif f.tkind == 'seg' and f.target == cs.name:
                    if sg != segp or off != f.disp + at:
                        problems.append((where, 'far ptr to own code +%x: original %04x:%04x' % (f.disp, sg, off)))
                else:
                    problems.append((where, 'far ptr to %s %s: unchecked (original %04x:%04x)' % (f.tkind, f.target, sg, off)))
            elif f.loc in (1, 5) and f.mode == 0 and f.tkind == 'seg' and f.target == cs.name:
                # self-relative (MASM's near jumps): target minus the end of the field
                v = ds_word(addr)
                if v != (f.disp - (f.offset + 2)) & 0xFFFF:
                    problems.append((where, 'relative to own code +%x: original %04x' % (f.disp, v)))
            elif f.loc in (1, 5):  # 16-bit offset
                v = ds_word(addr)
                if f.tkind in ('seg', 'grp') and f.target in ('_DATA', '_BSS', 'DGROUP', '_CVTSEG', '_SCNSEG'):
                    tgt = f.target
                    if f.tkind == 'grp':
                        # frame DGROUP, target given by the frame/target pair: treat as _DATA-relative
                        tgt = '_DATA'
                    b = bases.get(tgt)
                    if b is None:
                        bases[tgt] = v - f.disp
                    elif b + f.disp != v:
                        problems.append((where, 'offset %s+%x: original %04x, base %04x gives %04x' % (
                            tgt, f.disp, v, b, b + f.disp)))
                elif f.tkind == 'seg' and f.target == cs.name:
                    if v != f.disp + at:
                        problems.append((where, 'offset in own code +%x: original %04x' % (f.disp, v)))
                elif f.tkind == 'ext':
                    want = dsyms.get(f.target)
                    if want is None:
                        learned.append((f.target, '%04X' % (v - f.disp)))
                        dsyms[f.target] = v - f.disp
                    elif want + f.disp != v:
                        problems.append((where, 'offset %s+%x: original %04x, %s is at %04x' % (
                            f.target, f.disp, v, f.target, want)))
                else:
                    problems.append((where, 'offset to %s %s+%x unchecked (original %04x)' % (f.tkind, f.target, f.disp, v)))
            elif f.loc == 2:  # segment base
                v = ds_word(addr)
                want = DGROUP if f.target in ('DGROUP', '_DATA', '_BSS') else (segp if f.target == cs.name else None)
                if f.tkind == 'ext':
                    want = DGROUP if dsyms.get(f.target) is not None else syms.get(f.target, (None,))[0]
                if want is None or v != want:
                    problems.append((where, 'segment of %s %s: original %04x, expected %s' % (
                        f.tkind, f.target, v, '%04x' % want if want is not None else '?')))
            else:
                problems.append((where, 'fixup type %d unchecked' % f.loc))

    # ---- code ----
    fixmask = bytearray(cs.length)
    for f in cs.fixups:
        for k in range(f.size):
            if f.offset + k < cs.length:
                fixmask[f.offset + k] = 1
    first = None
    ndiff = 0
    for i in range(cs.length):
        if cs.mask[i] and not fixmask[i] and img[base + i] != cs.data[i]:
            ndiff += 1
            if first is None:
                first = i
    vote_bases(cs, base)
    check_fixups(cs, base)

    # ---- data ----
    dseg = [s for s in m.segs[1:] if s.name == '_DATA']
    dfirst = None
    if dseg and dseg[0].length:
        ds = dseg[0]
        if '_DATA' not in bases:
            problems.append((('_DATA', 0), 'no fixup locates this module\'s _DATA: give --data'))
        else:
            dbase = DGROUP * 16 + bases['_DATA']
            dmask = bytearray(ds.length)
            for f in ds.fixups:
                for k in range(f.size):
                    if f.offset + k < ds.length:
                        dmask[f.offset + k] = 1
            for i in range(ds.length):
                if dbase + i >= len(img):
                    dfirst = i
                    break
                if ds.mask[i] and not dmask[i] and img[dbase + i] != ds.data[i]:
                    dfirst = i
                    break
            check_fixups(ds, dbase)

    if a.learn:
        save_learned(learned)
    # ---- report ----
    funcs = func_starts(img[base:base + cs.length + 16], cs.length)
    pubs = sorted((o, n) for n, (si, o) in m.pubs.items() if m.segs[si] is cs)

    def func_at(off):
        name = None
        for o, n in pubs:
            if o <= off:
                name = '%s+%x' % (n, off - o)
        return name or '?'

    ok = first is None and not problems and dfirst is None
    print('%s: code %x bytes at %04x:0000, %d bytes differ; data %s; %d fixups checked, %d problems%s' % (
        os.path.basename(a.obj), cs.length, segp, ndiff,
        ('at DS:%04x %s' % (bases['_DATA'], 'differs at +%x' % dfirst if dfirst is not None else 'same'))
        if dseg and dseg[0].length and '_DATA' in bases else 'none',
        len(cs.fixups) + sum(len(s.fixups) for s in dseg), len(problems),
        ', learned %d names' % len(learned) if learned else ''))
    if '_BSS' in bases:
        print('  _BSS at DS:%04x' % bases['_BSS'])
    if a.data_range:
        dlen = dseg[0].length if dseg else 0
        span = int(a.data_range[1], 16) - int(a.data_range[0], 16)
        if dlen not in (span, span - 1):
            print('  DATA RANGE: module _DATA is %x bytes, the range is %x' % (dlen, span))
            ok = False
    if a.length:
        want = int(a.length, 16)
        if want != cs.length:
            print('  LENGTH: module is %x bytes, original segment %x' % (cs.length, want))
            ok = False
    # the first instruction that really differs: walk both disassemblies in step, taking
    # as equal the instructions that differ only under fixups or in a branch displacement
    real = None
    if first is not None:
        oblob = img[base:base + cs.length + 16]
        o_all = disasm(oblob, 0, 0, cs.length + 16, switch_tables(oblob, at))
        n_all = disasm(bytes(cs.data), 0, 0, cs.length, switch_tables(bytes(cs.data)))

        def parse(l):
            p = l.split(None, 2)
            return int(p[0], 16), bytes.fromhex(p[1]), p[2] if len(p) > 2 else ''
        k = 0
        while k < min(len(o_all), len(n_all)):
            oa, ob, oi = parse(o_all[k])
            na, nb, ni = parse(n_all[k])
            same = ob == nb
            if not same and len(ob) == len(nb):
                same = all(ob[j] == nb[j] or fixmask[na + j] for j in range(len(nb)) if na + j < cs.length)
            if not same and oi.split()[0] == ni.split()[0] and (oi.startswith(('j', 'call ', 'loop')) and
                                                               not oi.startswith('call far')):
                same = len(ob) == len(nb)
            if not same:
                real = (k, oa, na)
                break
            k += 1
        if real is None and len(n_all) > len(o_all):
            real = (len(o_all), parse(n_all[len(o_all)])[0], parse(n_all[len(o_all)])[0])
    cutoff = real[2] if real else (cs.length if first is None else first)
    shown = [(w, msg) for w, msg in problems if not (w[0] == cs.name and w[1] > cutoff)]
    hidden = len(problems) - len(shown)
    for (seg, off), msg in shown[:20]:
        print('  %s+%04x (%s): %s' % (seg, off, func_at(off) if seg == cs.name else '', msg))
    if hidden:
        print('  (%d more fixup problems after the first difference, probably the same shift)' % hidden)
    if real is not None:
        k, oa, na = real
        print('first real difference: original +%04x, yours +%04x (%s):' % (oa, na, func_at(na)))
        lo = max(0, k - 12)
        print('  original:                                     | yours:')
        for j in range(lo, k + 10):
            ol = o_all[j] if j < len(o_all) else ''
            nl = n_all[j] if j < len(n_all) else ''
            mark = '>' if j == k else ' '
            print(' %s%-46s | %s' % (mark, ol[:46], nl[:60]))
    elif first is not None:
        print('differences only in branch displacements/fixups: first byte at +%04x (%s)' % (first, func_at(first)))
    if a.v:
        for n, (si, o) in sorted(m.pubs.items(), key=lambda x: x[1][1]):
            print('  public %s %s+%x' % (n, m.segs[si].name, o))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
