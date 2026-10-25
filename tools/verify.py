"""Verify that a renamed source tree is code-equivalent to the original.

For every C translation unit, both trees are run through the preprocessor and the
resulting token streams are compared:
  * non-identifier tokens must be identical (integer/char constants may differ in
    spelling but must have identical value and C type under 16-bit Turbo C rules);
  * identifiers outside function bodies must be renamed exactly as the global map says;
  * identifiers inside a function body must either follow the global map or be a
    local/parameter/label of that function (per pycparser on the original), and the
    renaming inside each function must be a bijection.
Assembly files are compared token by token with case-insensitive application of
the global map (optionally with a leading underscore for cdecl names).

usage: verify.py ORIG_SRC NEW_SRC MAP.json [--quiet]
MAP.json: {"globals": {old: new}, "files": {old_relpath: new_relpath}}
"""
import json, os, re, sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pp, ctok, cscope

MAXLEN = 31
RESERVED = set('''auto break case char const continue default do double else enum extern float for goto if int long
register return short signed sizeof static struct switch typedef union unsigned void volatile while
asm cdecl far huge interrupt near pascal _cs _ds _es _ss
min max abs labs fabs random randomize rand srand time div ldiv exit atoi atol itoa ltoa ultoa malloc free calloc realloc
farmalloc farfree farcalloc farrealloc farcoreleft coreleft sound nosound delay sleep peek peekb poke pokeb inport inportb
outport outportb enable disable geninterrupt int86 int86x intdos intdosx segread MK_FP FP_SEG FP_OFF NULL EOF errno
strlen strcpy strncpy strcat strncat strcmp strncmp stricmp strnicmp strchr strrchr strstr strupr strlwr strdup memcpy
memset memcmp memmove movedata movmem setmem toupper tolower isalpha isdigit isspace isupper islower isalnum isxdigit
isprint iscntrl ispunct isascii toascii sqrt sin cos tan atan atan2 exp log log10 pow floor ceil fmod hypot
spawnl spawnv execl execv system main environ _psp _osmajor _osminor _version directvideo
'''.split())
ONLY = set()
MEMBER_ALTS = {}


def function_regions(toks):
    """list of (start_idx, end_idx_inclusive) of function definitions"""
    regions = []
    depth_p = depth_b = depth_c = 0
    decl_start = 0
    i = 0
    n = len(toks)
    while i < n:
        k, t = toks[i][0], toks[i][1]
        if k == 'punct':
            if t == '(':
                depth_p += 1
            elif t == ')':
                depth_p -= 1
            elif t == '[':
                depth_b += 1
            elif t == ']':
                depth_b -= 1
            elif t == '{':
                if depth_c == 0 and depth_p == 0 and toks[i - 1][1] == ')':
                    # function body
                    d = 0
                    j = i
                    while True:
                        if toks[j][1] == '{':
                            d += 1
                        elif toks[j][1] == '}':
                            d -= 1
                            if d == 0:
                                break
                        j += 1
                    regions.append((decl_start, j))
                    i = j + 1
                    decl_start = i
                    continue
                depth_c += 1
            elif t == '}':
                depth_c -= 1
            elif t == ';' and depth_c == 0 and depth_p == 0:
                decl_start = i + 1
        i += 1
    return regions


def same_const(a, b):
    va = ctok.int_value(a[0], a[1])
    vb = ctok.int_value(b[0], b[1])
    return va is not None and va == vb


def verify_c(orig_dir, new_dir, gmap, filemap, report):
    errors = []
    onlyo = None
    onlyn = None
    if ONLY:
        onlyo = set(ONLY)
        onlyn = set(filemap.get(x, x) for x in ONLY) | set(ONLY)
    po = pp.preprocess_tree(orig_dir, only=onlyo)
    pn = pp.preprocess_tree(new_dir, only=onlyn)
    po_parse = pp.preprocess_tree(orig_dir, cscope.PARSE_DEFS, only=onlyo)
    pn_parse = pp.preprocess_tree(new_dir, cscope.PARSE_DEFS, only=onlyn)
    all_globals = set()
    for tu in po:
        if ONLY and tu not in ONLY and os.path.basename(tu) not in ONLY:
            continue
        ntu = filemap.get(tu, tu)
        if ntu not in pn:
            errors.append('missing translation unit %s (was %s)' % (ntu, tu))
            continue
        an = cscope.analyze(po_parse[tu], tu)
        try:
            cscope.analyze(pn_parse[ntu], ntu)
        except Exception as e:
            errors.append('%s: new tree does not parse: %s' % (ntu, e))
        all_globals |= an['globals']
        to = ctok.sig_tokens(po[tu])
        tn = ctok.sig_tokens(pn[ntu])
        regs = function_regions(to)
        if len(regs) != len(an['funcs']):
            errors.append('%s: region/function count mismatch %d vs %d' % (tu, len(regs), len(an['funcs'])))
            continue
        if len(to) != len(tn):
            # find first divergence for diagnostics
            for idx in range(min(len(to), len(tn))):
                a, b = to[idx], tn[idx]
                if a[0] != b[0] or (a[0] != 'id' and a[1] != b[1] and not same_const(a, b)):
                    ctx_o = ' '.join(x[1] for x in to[max(0, idx - 8):idx + 8])
                    ctx_n = ' '.join(x[1] for x in tn[max(0, idx - 8):idx + 8])
                    errors.append('%s: token count differs (%d vs %d); first divergence:\n   orig: %s\n   new : %s' % (tu, len(to), len(tn), ctx_o, ctx_n))
                    break
            else:
                errors.append('%s: token count differs (%d vs %d) at end' % (tu, len(to), len(tn)))
            continue
        # region index per token
        reg_of = [-1] * len(to)
        for ri, (s, e) in enumerate(regs):
            for x in range(s, e + 1):
                reg_of[x] = ri
        fwd = defaultdict(dict)
        inv = defaultdict(dict)
        for idx, (a, b) in enumerate(zip(to, tn)):
            if a[0] != b[0] and not same_const(a, b):
                errors.append('%s: token kind differs at %d: %r vs %r' % (tu, idx, a[1], b[1]))
                continue
            if a[0] != 'id':
                if a[1] != b[1] and not same_const(a, b):
                    errors.append('%s: token differs at %d: %r vs %r' % (tu, idx, a[1], b[1]))
                continue
            o, nn = a[1], b[1]
            g = gmap.get(o, o)
            ri = reg_of[idx]
            if ri < 0:
                if nn != g and nn not in MEMBER_ALTS.get(o, ()):
                    ctx = ' '.join(x[1] for x in tn[max(0, idx - 6):idx + 6])
                    errors.append('%s: top-level identifier %s -> %s, map says %s   [%s]' % (tu, o, nn, g, ctx))
                continue
            f = an['funcs'][ri]
            prev = to[idx - 1][1] if idx else ''
            if prev in ('.', '->', 'struct', 'union', 'enum'):
                # member / tag namespace: always global
                if nn != g and nn not in MEMBER_ALTS.get(o, ()):
                    errors.append('%s:%s: member/tag %s renamed to %s (map: %s)' % (tu, f['name'], o, nn, g))
                continue
            if o in fwd[ri]:
                if fwd[ri][o] != nn:
                    errors.append('%s:%s: identifier %s renamed inconsistently (%s / %s)' % (tu, f['name'], o, fwd[ri][o], nn))
                continue
            if nn != g and o not in f['locals']:
                errors.append('%s:%s: non-local identifier %s renamed to %s (map: %s)' % (tu, f['name'], o, nn, g))
                continue
            if nn in inv[ri] and inv[ri][nn] != o:
                errors.append('%s:%s: name clash, both %s and %s become %s' % (tu, f['name'], inv[ri][nn], o, nn))
                continue
            fwd[ri][o] = nn
            inv[ri][nn] = o
            if nn != o and o in f['locals'] and nn != g:
                if nn in RESERVED or nn.startswith('_'):
                    errors.append('%s:%s: local name %s is reserved / library name' % (tu, f['name'], nn))
                if len(nn) > MAXLEN:
                    errors.append('%s:%s: local name too long: %s' % (tu, f['name'], nn))
        if report is not None:
            for ri, f in enumerate(an['funcs']):
                ren = {o: v for o, v in fwd[ri].items() if o in f['locals'] and v != o}
                same = sorted(o for o in f['locals'] if fwd[ri].get(o, o) == o)
                report.append((tu, f['name'], gmap.get(f['name'], f['name']), ren, same))
    return errors, all_globals


# ---------------------------------------------------------------- assembly
_asm_tok = re.compile(r"""(;[^\n]*)|('[^'\n]*'|"[^"\n]*")|([A-Za-z_@$?][A-Za-z0-9_@$?]*)|([0-9][0-9A-Fa-f]*[hHbBoOdD]?)|(\s+)|(.)""")


def asm_tokens(src):
    out = []
    for m in _asm_tok.finditer(src):
        if m.group(1) or m.group(5):
            continue
        if m.group(3):
            out.append(('id', m.group(3)))
        else:
            out.append(('x', m.group(0)))
    return out


def asm_map(gmap):
    low = {}
    for o, n in gmap.items():
        low[o.lower()] = n
        low['_' + o.lower()] = '_' + n
    return low


def verify_asm(orig_dir, new_dir, gmap, filemap):
    errors = []
    low = asm_map(gmap)
    for root, dirs, files in os.walk(orig_dir):
        for f in files:
            if not f.upper().endswith(('.ASM', '.INC')):
                continue
            rel = os.path.relpath(os.path.join(root, f), orig_dir)
            nrel = filemap.get(rel, rel)
            np_ = os.path.join(new_dir, nrel)
            if not os.path.exists(np_):
                errors.append('missing asm file %s' % nrel)
                continue
            a = asm_tokens(open(os.path.join(root, f), encoding='latin-1').read())
            b = asm_tokens(open(np_, encoding='latin-1').read())
            if len(a) != len(b):
                errors.append('%s: asm token count differs' % rel)
                continue
            for x, y in zip(a, b):
                if x[0] != y[0]:
                    errors.append('%s: asm token kind differs %r %r' % (rel, x, y)); break
                if x[0] == 'id':
                    exp = low.get(x[1].lower())
                    if exp is None:
                        if x[1] != y[1]:
                            errors.append('%s: asm identifier %s changed to %s (not in map)' % (rel, x[1], y[1]))
                    elif y[1].lower() != exp.lower():
                        errors.append('%s: asm identifier %s -> %s, expected %s' % (rel, x[1], y[1], exp))
                elif x[1] != y[1]:
                    errors.append('%s: asm token %r -> %r' % (rel, x[1], y[1]))
    return errors


def check_map(gmap, all_globals):
    errors = []
    images = defaultdict(set)
    for o in all_globals | set(gmap):
        images[gmap.get(o, o)].add(o)
    for n, os_ in images.items():
        if len(os_) > 1:
            errors.append('map not injective: %s <- %s' % (n, sorted(os_)))
    ci = defaultdict(set)
    for n in images:
        ci[n.lower()].add(n)
    for k, v in ci.items():
        if len(v) > 1:
            errors.append('case-insensitive collision: %s' % sorted(v))
    for o, n in gmap.items():
        if len(n) > MAXLEN:
            errors.append('global name too long (%d): %s' % (len(n), n))
        if not re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', n):
            errors.append('bad identifier: %r' % n)
    return errors


def main():
    global ONLY
    orig, new, mapf = sys.argv[1:4]
    if '--only' in sys.argv:
        ONLY = set(sys.argv[sys.argv.index('--only') + 1].split(','))
    m = json.load(open(mapf)) if os.path.exists(mapf) else {}
    gmap = m.get('globals', {})
    MEMBER_ALTS.update(m.get('member_alts', {}))
    filemap = m.get('files', {})
    report = []
    errs, all_globals = verify_c(orig, new, gmap, filemap, report)
    if not ONLY:
        errs += verify_asm(orig, new, gmap, filemap)
        errs += check_map(gmap, all_globals)
    if '--report' in sys.argv:
        out = sys.argv[sys.argv.index('--report') + 1]
        with open(out, 'w') as fh:
            for tu, fo, fn, ren, same in report:
                fh.write('%s %s -> %s\n' % (tu, fo, fn))
                for o, v in sorted(ren.items()):
                    fh.write('    %s -> %s\n' % (o, v))
                if same:
                    fh.write('    unchanged: %s\n' % ' '.join(same))
    for e in errs[:300]:
        print('ERROR', e)
    print('%d errors' % len(errs))
    sys.exit(1 if errs else 0)


if __name__ == '__main__':
    main()
