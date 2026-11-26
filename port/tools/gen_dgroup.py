#!/usr/bin/env python3
"""Generate how the port writes GAME.EXE's DGROUP in DOS's layout, for the DATA and BSS sections
of Exhume's replay dumps (port/include/rpport.h; PLAN-exact-port.md, Phase 2 task 2):

    python3 port/tools/gen_dgroup.py --nm NM.TXT [--map build/dos/replay/RGAME.MAP] [--report]

writes port/game/dgroup_port.c (the table), port/game/tu_<name>.c (a game source compiled with
a table of its statics) and port/game/dgs_<file>.inc (the same for a port file, included at its
end).

The port keeps the game's near data as host globals: BSS.ASM's block in DOS's order
(port/sys/bss.c), gamestat.c's variables as a packed block at DOS's offsets, and the other
initialised data one variable at a time, wherever the host links it. The replay DOS build dumps
DGROUP as one image, DS:0..EDF9. The table says, for each DOS byte range the port has a
counterpart for, where that is and how to write it in DOS's form:

    BYTES  copy the bytes (no pointers: the game's types have DOS's widths)
    FAR    a host pointer, written as DOS's far pointer (offset, segment): into the DOS arena, its
           segment and offset there (normalised, as FP_SEG and FP_OFF give them); into the port's
           near data, segment 0 (the port's DGROUP: SEGS' first word) and the DOS offset of the
           byte it points at; NULL, 0:0
    NEAR   a host pointer into the near data, written as that byte's DOS offset
    CODE   a host function pointer, written as DOS's far code pointer: the function's offset in
           RGAME.MAP and its segment counted from DGROUP's paragraph (329Bh), which is what a
           segment that moves with the load segment is next to the port's DGROUP of 0
    RAND   RAND.ASM's two words, which rand.c keeps as statics (u6_rand_seed)

A pointer the port cannot place (to a string literal, which is host memory; to a static
function) is written as FFFFh words. Everything the table leaves out is written as zeros:
Turbo C's run-time data, the game's string literals, the 8087 emulator's and INFLATE.ASM's work
variables. --report lists those holes; exhume.toml masks them, each with its reason.

Where the names come from:
- DATA publics: RGAME.MAP's DGROUP publics (upper case: GAME is compiled -p), matched to the
  port's symbols without regard to case (NM.TXT, `nm -S --defined-only` of a port build, which
  gives each one's size);
- DATA statics: the DOS offsets the game's sources give in comments (`/*049B*/static ...`), and
  the port's own files' statics that name their DOS counterpart (`/* D_2FB4 */`);
- BSS: port/sys/bss.c's table, each object's DOS offset counted from 322Eh in BSS.ASM's order
  (u6.h's offset comments agree with every one).
Run it again when a near global of the port changes its type or is added.
"""
import argparse, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
GAME = os.path.join(ROOT, 'port', 'game')
BSS_BASE = 0x322E
DATA_END = 0x322E
DGROUP_END = 0xEDFA
DGROUP_PARA = 0x329B
HOST_PTR = 8
DOS_SIZE = {'NEAR': 2, 'FAR': 4, 'CODE': 4, 'HGR': 4, 'HMUS': 4}

# DATA publics that hold pointers, by RGAME.MAP name
DATA_PTR = {
    # near pointers to string literals (constant tables)
    'BITMAPFILES': 'NEAR', 'DIRNAMES': 'NEAR', 'WINDDIRNAMES': 'NEAR', 'COMPRESSEDFILELIST': 'NEAR',
    'SPELLCODE': 'NEAR', 'MAGICWORDS': 'NEAR', 'SPELLNAME': 'NEAR', 'REAGENTS_NAME': 'NEAR',
    'CIRCLENAMES': 'NEAR', 'ALIGNMENTNAMES': 'NEAR', 'SPELLMESSAGES': 'NEAR', 'POTIONNAMES': 'NEAR',
    'DUNGEONNAMES': 'NEAR', 'MANTRAS': 'NEAR', 'FOUNTAINWISHES': 'NEAR', 'MUS_MELODIES': 'NEAR',
    'MUS_SONGFILES': 'NEAR', 'GRDRIVERFILES': 'NEAR', 'DISKNAMES': 'NEAR', 'POINTERFILES': 'NEAR',
    'MUS_DRIVERFILES': 'NEAR',
    # near pointers that change: the last file name, the clip rectangle
    'OSI_ERRORFILE': 'NEAR', 'CURCLIP': 'NEAR',
    # far code pointers
    'OSI_ERRORHANDLER': 'CODE', 'CON_PUTCHARFN': 'CODE', 'CON_SCROLLUPFN': 'CODE',
    'DRIVEERRORCALLBACK': 'CODE',
}

# BSS objects whose port form differs from DOS's: (port offset, DOS offset, kind[, bytes])
TSURFACE = [(0, 0, 'BYTES', 2), (2, 2, 'FAR'), (10, 6, 'BYTES', 8)]
BSS_FIELDS = {
    'Mouse': [(0, 0, 'BYTES', 8), (8, 8, 'FAR'), (16, 12, 'BYTES', 2), (18, 14, 'NEAR'),
              (26, 16, 'FAR'), (34, 20, 'CODE'), (42, 24, 'BYTES', 1)],
    'Screen': TSURFACE, 'MapSurface': TSURFACE,
    'MouseArea': TSURFACE + [(18, 14, 'BYTES', 1)],
    'PointerShapes': [(0, 0, 'FAR'), (8, 4, 'BYTES', 1)],
    'VarStr': [(i * 8, i * 2, 'NEAR') for i in range(36)],
    # union tHandler: DOS's offset word is the entry number of the last call (gr.h, u6port.h),
    # which the port's call macros keep in u6_gr_entry and u6_mus_entry
    'GrDriver': [(0, 0, 'HGR')], 'MUS_Driver': [(0, 0, 'HMUS')],
}

# the port files whose statics name a DOS variable (/* D_XXXX */), and what each one is
PORT_FILES = ['sys/mouse.c', 'sys/osi.c', 'sys/sound.c']

# static functions a far code pointer in the near data may name (not public, so not in
# RGAME.MAP): game source -> [(name, RGAME.MAP segment, offset)], the address the replay DOS
# build's dumps show
STATIC_FUNCS = {
    'disk': [('DISK_NullCallback', 0x2A91, 0x0003)],   # DriveErrorCallback (disk.c, DISK_init)
}


def parse_bss():
    objs, off = [], BSS_BASE
    for l in open(os.path.join(ROOT, 'port', 'sys', 'bss.c')):
        m = re.match(r'AT\((\w+), (\d+)\) /\* (\d+) bytes \(DOS (\d+)\): (.*?) \*/', l)
        if m:
            name, poff, psize, dsize, typ = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5)
            objs.append((name, poff, psize, off, dsize, typ))
            off += dsize
    return objs


def parse_map(path):
    t = open(path, encoding='latin-1').read()
    data = sorted({(int(o, 16), n) for o, n in re.findall(r'\b329B:([0-9A-F]{4})\s+(\S+)', t)})
    code = {}
    for s, o, n in re.findall(r'\b([0-9A-F]{4}):([0-9A-F]{4})\s+(\S+)', t):
        s, o = int(s, 16), int(o, 16)
        if s < DGROUP_PARA:
            code.setdefault(n.upper(), (s, o))
    return data, code


def parse_nm(path):
    syms = {}
    for l in open(path):
        p = l.split()
        if len(p) == 4 and re.fullmatch(r'[0-9a-f]+', p[1]):
            addr, size, typ, name = p
        elif len(p) == 3 and re.fullmatch(r'[0-9a-f]+', p[0]):
            addr, typ, name = p
            size = '0'
        else:
            continue
        syms.setdefault(name.upper(), []).append((name, int(size, 16), typ))
    return syms


def decl_kind(decl):
    """the kind of a declaration's elements: NEAR or FAR for a pointer (medium model: near
    unless declared far), else BYTES"""
    head = decl.split('=')[0]
    if '*' in head:
        return 'FAR' if re.search(r'\bfar\b', head) else 'NEAR'
    return 'BYTES'


def decl_name(decl):
    head = decl.split('=')[0]
    m = re.search(r'(\w+)\s*(?:\[[^\]]*\]\s*)*$', head.strip())
    return m.group(1) if m else None


def game_statics():
    """{tu: [(name, dos_off, kind)]}: the game's statics with a DOS offset comment"""
    out = {}
    for f in sorted(glob.glob(os.path.join(ROOT, 'SRC', '*.c'))):
        tu = os.path.splitext(os.path.basename(f))[0]
        for l in open(f, encoding='latin-1'):
            m = re.match(r'^/\*([0-9A-F]{4})\*/\s*static\s+(.*?)(;|=|$)', l)
            if not m:
                continue
            off = int(m.group(1), 16)
            if off >= DATA_END:
                continue
            decl = m.group(2) + (m.group(3) if m.group(3) == '=' else '')
            n = decl_name(m.group(2))
            if n:
                out.setdefault(tu, []).append((n, off, decl_kind(m.group(2))))
    return out


def port_statics():
    """{file: [(name, dos_off, kind)]}: the port's statics that name a DOS variable"""
    out = {}
    for rel in PORT_FILES:
        for l in open(os.path.join(ROOT, 'port', rel)):
            m = re.match(r'^static\s+([^;(]*?);\s*/\*.*?\bD_([0-9A-F]{4})\b', l)
            if not m:
                continue
            decl, off = m.group(1), int(m.group(2), 16)
            if off in (0x3088, 0x308A, 0x305A):        # RAND (u6_rand_seed); DELAY's tick count
                continue
            out.setdefault(rel, []).append((decl_name(decl.split('=')[0]), off, decl_kind(decl)))
    return out


def writable_strs():
    """[(DOS offset, C string literal)]: the literals the game writes into, WRITABLE_STR(/*XXXX*/"...")"""
    out = []
    for f in sorted(glob.glob(os.path.join(ROOT, 'SRC', '*.c'))):
        for m in re.finditer(r'WRITABLE_STR\(\s*/\*([0-9A-F]{4})\*/\s*("(?:[^"\\]|\\.)*")\s*\)', open(f, encoding='latin-1').read()):
            out.append((int(m.group(1), 16), m.group(2)))
    return out


def dgs_name(key):
    return 'u6_dgs_' + re.sub(r'\W', '_', key.replace('.c', ''))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--nm', required=True, help='nm -S --defined-only of a port build')
    ap.add_argument('--map', default=os.path.join(ROOT, 'build', 'dos', 'replay', 'RGAME.MAP'))
    ap.add_argument('--report', action='store_true', help='print the ranges the table leaves out')
    ap.add_argument('--masks', action='store_true',
                    help="write the cross masks of what the table leaves out into exhume.toml's marked block")
    a = ap.parse_args()
    bss = parse_bss()
    data, code = parse_map(a.map)
    nm = parse_nm(a.nm)
    gst, pst = game_statics(), port_statics()
    for tu in STATIC_FUNCS:
        gst.setdefault(tu, [])
    ents = []          # (dos_off, kind, dos_len or gap, host expr or None, ref expr or None, note)
    externs = set()

    # every DOS offset the sources or the map name, for the gaps
    known = sorted({o for o, _ in data if o < DATA_END} |
                   {o for v in gst.values() for _, o, _ in v} |
                   {o for v in pst.values() for _, o, _ in v} | {0x2C4A, 0x2CCE, DATA_END})

    def gap(o):
        nxt = [k for k in known if k > o]
        return (nxt[0] if nxt else DATA_END) - o

    def data_sym(n):
        for name, size, typ in nm.get(n.upper(), []):
            if typ in 'BDRGS':
                return name, size
        return None

    for o, n in data:
        if o >= DATA_END or n.startswith('_') or n.startswith('EMWS_'):
            continue                                  # Turbo C's run-time data
        if n == 'D_3088':
            ents.append((o, 'RAND', 4, '0', None, 'D_3088, D_308A'))
            continue
        if 0x2C4A <= o < 0x2CCE:
            continue                                  # gamestat.c's packed block, below
        s = data_sym(n)
        if not s:
            continue
        name, size = s
        externs.add(name)
        kind = DATA_PTR.get(n.upper())
        if kind:
            for k in range(size // HOST_PTR):
                ents.append((o + k * DOS_SIZE[kind], kind, DOS_SIZE[kind], '(char *)%s + %d' % (name, k * HOST_PTR), None, n))
            continue
        g = gap(o)
        if size > g:
            print('gen_dgroup: %s (%04X): the port\'s %d bytes are more than DOS\'s %d' % (name, o, size, g), file=sys.stderr)
        ents.append((o, 'BYTES', min(size, g), name, None, n))
    ents.append((0x2C4A, 'BYTES', 0x84, 'u6_gamestat', None, 'gamestat.c'))
    externs.add('u6_gamestat')

    # statics, through their translation unit's table
    tables = {}
    for key, lst in list(gst.items()) + list(pst.items()):
        tables[key] = lst
        for k, (name, o, kind) in enumerate(lst):
            ents.append((o, 'S' + kind, gap(o), None, '&%s[%d]' % (dgs_name(key), k), name))

    for name, poff, psize, doff, dsize, typ in bss:
        f = BSS_FIELDS.get(name)
        if f is None and psize != dsize:
            if psize == HOST_PTR and dsize == 4:
                f = [(0, 0, 'FAR')]
            else:
                sys.exit('gen_dgroup: %s: port %d bytes, DOS %d, and no field list' % (name, psize, dsize))
        if f is None:
            ents.append((doff, 'BYTES', dsize, 'u6_bss + %d' % poff, None, name))
            continue
        for fl in f:
            ln = fl[3] if fl[2] == 'BYTES' else DOS_SIZE[fl[2]]
            ents.append((doff + fl[1], fl[2], ln, 'u6_bss + %d' % (poff + fl[0]), None, name))

    ents.sort(key=lambda e: e[0])
    funcs = []
    for n, (s, o) in sorted(code.items()):
        # every port function whose name is the DOS one without regard to case (GAME is -p:
        # OSI_READ is OSI_read; the port also has an OSI_Read). The assembly library's C names
        # carry Turbo C's underscore (_MOUSE_0055), which the port's do not
        key = n[1:] if n.startswith('_') and n[1:] not in code else n
        for name, size, typ in nm.get(key, []):
            if typ == 'T':
                funcs.append((name, (s - DGROUP_PARA) & 0xFFFF, o))

    w = ['/* Generated by port/tools/gen_dgroup.py from RGAME.MAP, port/sys/bss.c, the sources\' DOS offset',
         ' * comments and a port build\'s symbols: do not edit. GAME.EXE\'s DGROUP (DS:0..EDF9) in DOS\'s',
         ' * layout, written from the port\'s own globals for the DATA and BSS sections of Exhume\'s replay',
         ' * dumps (port/include/rpport.h; gen_dgroup.py says how). */',
         '#include <stdint.h>', '#include "dgs.h"', '',
         'extern uint8_t u6_bss[];']
    for x in sorted(externs):
        w.append('extern char %s[];' % x)
    for key in sorted(tables):
        w.append('extern const struct u6_dgs %s[];' % dgs_name(key))
    for tu in sorted(STATIC_FUNCS):
        w.append('extern const struct dg_fn u6_dgf_%s[];' % tu)
    for name, s, o in funcs:
        if name not in externs:
            w.append('extern void %s();' % name)
    w += ['', 'const struct dg_ent u6_dg_ents[] = {']
    for o, kind, ln, expr, ref, note in ents:
        w.append('\t{0x%04X, %d, K_%s, %s, %s},\t/* %s */' % (o, ln, kind, expr or '0', ref or '0', note))
    w += ['};', 'const unsigned u6_dg_nents = sizeof u6_dg_ents / sizeof u6_dg_ents[0];', '',
          '/* the functions a far code pointer may name: RGAME.MAP\'s offset, the segment from DGROUP\'s paragraph */',
          'const struct dg_fn u6_dg_fns[] = {']
    for name, s, o in funcs:
        w.append('\t{(const void *)%s, 0x%04X, 0x%04X},' % (name, s, o))
    w += ['};', 'const unsigned u6_dg_nfns = sizeof u6_dg_fns / sizeof u6_dg_fns[0];', '',
          '/* the string literals the game writes into (WRITABLE_STR): their text and DOS offset */',
          'const struct dg_wstr u6_dg_wstrs[] = {']
    w += ['\t{%s, 0x%04X},' % (lit, o) for o, lit in writable_strs()]
    w += ['\t{0, 0}', '};', '',
          '/* the static functions, each game source\'s table (port/game/tu_NAME.c), ended by a 0 */',
          'const struct dg_fn *const u6_dg_sfns[] = {%s0};' % ''.join('u6_dgf_%s, ' % tu for tu in sorted(STATIC_FUNCS)),
          'const unsigned u6_dg_nsfns[] = {%s0};' % ''.join('%d, ' % len(STATIC_FUNCS[tu]) for tu in sorted(STATIC_FUNCS))]
    open(os.path.join(GAME, 'dgroup_port.c'), 'w', newline='\n').write('\n'.join(w) + '\n')

    for tu, lst in sorted(gst.items()):
        t = ['/* Generated by port/tools/gen_dgroup.py: do not edit. SRC/%s.c, compiled with a table of' % tu,
             ' * its statics (their DOS offsets are in its comments) for port/game/dgroup_port.c. */',
             '#include "%s.c"' % tu, '#include "dgs.h"', '',
             'const struct u6_dgs %s[] = {' % dgs_name(tu)]
        t += ['\t{&%s, sizeof %s, 0},\t/* %04X */' % (n, n, o) for n, o, _ in lst]
        if not lst:
            t += ['\t{0, 0, 0}']
        t += ['};']
        if tu in STATIC_FUNCS:
            t += ['', '/* its static functions a far code pointer may name: RGAME.MAP\'s segment (from DGROUP\'s',
                  ' * paragraph) and offset, as the replay DOS build\'s dumps show them */',
                  'const struct dg_fn u6_dgf_%s[] = {' % tu]
            t += ['\t{(const void *)%s, 0x%04X, 0x%04X},' % (n, (s - DGROUP_PARA) & 0xFFFF, o) for n, s, o in STATIC_FUNCS[tu]]
            t += ['};']
        open(os.path.join(GAME, 'tu_%s.c' % tu), 'w', newline='\n').write('\n'.join(t) + '\n')
    for rel, lst in sorted(pst.items()):
        base = os.path.splitext(os.path.basename(rel))[0]
        t = ['/* Generated by port/tools/gen_dgroup.py: do not edit. port/%s\'s statics that stand for' % rel,
             ' * GAME.EXE\'s near data, for port/game/dgroup_port.c (the file includes this at its end). */',
             '#include "../include/dgs.h"', '',
             'const struct u6_dgs %s[] = {' % dgs_name(rel)]
        t += ['\t{&%s, sizeof %s, 0},\t/* D_%04X */' % (n, n, o) for n, o, _ in lst]
        t += ['};']
        open(os.path.join(GAME, 'dgs_%s.inc' % base), 'w', newline='\n').write('\n'.join(t) + '\n')

    print('gen_dgroup: %d entries, %d game sources and %d port files with statics, %d functions' % (
        len(ents), len(gst), len(pst), len(funcs)))
    if a.masks:
        write_masks(ents, nm, data, gst)
    if a.report:
        holes, pos = [], 0
        for e in ents:
            if e[0] > pos:
                holes.append((pos, e[0]))
            ln = e[2] if not e[1].startswith('S') else 1
            pos = max(pos, e[0] + ln)
        if pos < DGROUP_END:
            holes.append((pos, DGROUP_END))
        names = {o: n for o, n in data}
        for lo, hi in holes:
            inside = [names[o] for o in sorted(names) if lo <= o < hi]
            print('  hole %04X..%04X (%d): %s' % (lo, hi - 1, hi - lo, ' '.join(inside[:6]) + (' ...' if len(inside) > 6 else '')))


# why the port has no counterpart for a hole, by where it lies
HOLE_WHY = [
    (0x0000, 0x0196, "Turbo C 2.0's run-time data: C0's variables (the arguments, the environment, the PSP, "
                     "the heap's and the break's levels) and the 8087 emulator's work area (EMWS_*)"),
    (0x2F90, 0x2FC6, "INFLATE.ASM's LZW decoder variables (its masks, the hash and output segments, the bit "
                     "position, the codes), left from the last decompression; port/sys/lzw.c is checked "
                     "against INFLATE.ASM in Unicorn (port/tests/osilib_diff.py LZW), work area included"),
    (0x2FC6, 0x3088, "OSILIB's own variables the port keeps another way or not at all (LOW.ASM's drive, error "
                     "and video state, DELAY.ASM's tick count, 0 in the replay build, DELAY2's counters, the "
                     "\"Out of Memory\" text)"),
    (0x308C, 0x3099, "SOUND1.ASM's and SOUND4.ASM's delay loop counters (the port times the loops itself)"),
    (0x3099, 0x322E, "Turbo C 2.0's library data: the DOS error tables, the exit and atexit state, the heap's "
                     "bookkeeping, the stream table and the ctype table"),
    (0xECCA, 0xEDFA, "Turbo C 2.0's library BSS: the atexit table and the near heap's first, last and rover "
                     "pointers"),
]
HOLE_DEFAULT = ("the module's string literals (Turbo C puts them in _DATA after its variables) and unnamed "
                "data; constant, and the port's literals are host memory")


def static_size(nm, name, gap):
    for n, size, typ in nm.get(name.upper(), []):
        if n == name and typ in 'bdrBDR' and 0 < size:
            return min(size, gap)
    return gap


def write_masks(ents, nm, data, gst):
    """the cross masks: every DGROUP byte range the table leaves out, and the near pointers to
    string literals, written into exhume.toml between the markers"""
    spans = []
    for o, kind, ln, expr, ref, note in ents:
        if kind.startswith('S'):
            ln = static_size(nm, note, ln) if kind == 'SBYTES' else ln
        spans.append((o, o + ln))
    for o, lit in writable_strs():             # the game's writable literals: the port writes them
        import ast
        spans.append((o, o + len(ast.literal_eval(lit).encode('latin-1')) + 1))
    spans.sort()
    holes, pos = [], 0x31                     # DS:0..30h is written (C0's copyright)
    for lo, hi in spans:
        if lo > pos:
            holes.append((pos, lo))
        pos = max(pos, hi)
    if pos < DGROUP_END:
        holes.append((pos, DGROUP_END))
    # split holes at the reason boundaries
    cuts = sorted({b for lo, hi, _ in HOLE_WHY for b in (lo, hi)})
    parts = []
    for lo, hi in holes:
        edges = [lo] + [c for c in cuts if lo < c < hi] + [hi]
        parts += list(zip(edges, edges[1:]))
    out = ['# ---- generated by port/tools/gen_dgroup.py --masks: do not edit by hand ----',
           '# The DOS bytes of DGROUP the port has no counterpart for (port/game/dgroup_port.c leaves',
           '# them 0), and its near pointers to string literals (FFFFh in the port). Cross only: two DOS',
           '# runs compare them.']
    for lo, hi in parts:
        why = next((w for a, b, w in HOLE_WHY if a <= lo < b), HOLE_DEFAULT)
        sec, base = ('DATA', 0) if lo < DATA_END else ('BSS ', DATA_END)
        if lo < DATA_END < hi:
            raise SystemExit('gen_dgroup: a hole across DATA and BSS')
        out += ['[[replay.mask]]', 'section = "%s"' % sec.strip(), 'lo = 0x%04X' % (lo - base),
                'hi = 0x%04X' % (hi - base), 'scope = "cross"', 'why = "%s"' % why.replace('"', '\\"'), '']
    lit = {}
    for o, kind, ln, expr, ref, note in ents:
        if kind in ('NEAR', 'SNEAR') and note.upper() not in ('OSI_ERRORFILE', 'CURCLIP') and note != 'VarStr' \
                and note != 'Mouse':
            n = ln if kind == 'NEAR' else ln
            lo, hi = lit.get(note, (o, o))
            lit[note] = (min(lo, o), max(hi, o + n))
    for note, (lo, hi) in sorted(lit.items(), key=lambda x: x[1]):
        out += ['[[replay.mask]]', 'section = "DATA"', 'lo = 0x%04X' % lo, 'hi = 0x%04X' % hi, 'scope = "cross"',
                'why = "%s: near pointers to string literals (constant); the port\'s literals are host memory"' % note, '']
    out.append('# ---- end of the generated masks ----')
    toml = os.path.join(ROOT, 'exhume.toml')
    t = open(toml, newline='').read()
    b, e = '# ---- generated by port/tools/gen_dgroup.py --masks', '# ---- end of the generated masks ----'
    if b in t:
        i, j = t.index(b), t.index(e) + len(e)
        t = t[:i] + '\n'.join(out) + t[j:]
    else:
        t = t.rstrip('\n') + '\n\n' + '\n'.join(out) + '\n'
    open(toml, 'w', newline='').write(t)
    print('gen_dgroup: %d hole masks, %d literal table masks -> exhume.toml' % (len(parts), len(lit)))


if __name__ == '__main__':
    main()
