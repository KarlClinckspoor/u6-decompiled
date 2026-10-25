"""Apply a global rename map to a source tree, in place.

usage: rename.py TREE MAP.json [MAP2.json ...]
MAP.json: {"globals": {old: new}, "files": {old_relpath: new_relpath}}

* C/H: identifier tokens (code and preprocessor lines) are replaced; in comments,
  whole-word occurrences of "technical" old names (containing a digit or '_') are
  replaced, except pure address markers like /*C_1184_0073*/.
  When a top-level declaration/definition of an address-named symbol (C_xxxx_xxxx,
  __xxxx_xxxx, D_xxxx) is renamed, an address marker comment is prepended so the
  original address stays visible.
* ASM/INC: identifiers outside ';' comments are replaced case-insensitively
  (also with a leading '_' for cdecl names).
* files are renamed (git mv if TREE is in a git repo) and #include "..." updated.
"""
import json, os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ctok

ADDR_FN = re.compile(r'^(?:C|__)_?([0-9A-F]{4})_([0-9A-F]{4})$')
ADDR_D = re.compile(r'^D_([0-9A-F]{4})$')
PURE_MARKER = re.compile(r'^/\*\s*(?:(?:C_|__)[0-9A-Fa-f]{4}_[0-9A-Fa-f]{4}|[A-Z]+_[0-9A-F]{4})\s*\*/$')
TOPLEVEL_START = re.compile(r'^(static|extern|int|unsigned|signed|char|void|struct|union|far|long|short|cdecl|const|[A-Za-z_]\w*\s*\()')


def load_maps(paths):
    g, f = {}, {}
    for p in paths:
        m = json.load(open(p))
        g.update(m.get('globals', {}))
        f.update(m.get('files', {}))
    # identity entries are pointless
    g = {k: v for k, v in g.items() if k != v}
    return g, f


PLACEHOLDER_D = re.compile(r'^D_([0-9A-Fa-f]{4})(?:_([0-9A-Fa-f]{4}))?$')
PLACEHOLDER_FN = re.compile(r'^(?:(?:C_|__)[0-9A-Fa-f]{4}_[0-9A-Fa-f]{4}|[A-Z]+_[0-9A-F]{4})$')


def _dmark(n):
    m = PLACEHOLDER_D.match(n)
    return m.group(1) + (':' + m.group(2) if m.group(2) else '')


def add_markers(src, gmap):
    """prepend address markers on top-level declaration lines of renamed placeholder symbols"""
    lines = src.split('\n')
    starts = []
    pos = 0
    for l in lines:
        starts.append(pos)
        pos += len(l) + 1
    depth = 0
    depth_at_line = [0] * len(lines)
    li = 0
    for t in ctok.tokenize(src):
        while li + 1 < len(lines) and starts[li + 1] <= t[2]:
            li += 1
            depth_at_line[li] = depth
        if t[0] == 'punct' and t[1] == '{':
            depth += 1
        elif t[0] == 'punct' and t[1] == '}':
            depth -= 1
    while li + 1 < len(lines):
        li += 1
        depth_at_line[li] = depth
    # lines that start inside a comment
    in_comment_at = [False] * len(lines)
    for t in ctok.tokenize(src):
        if t[0] == 'comment':
            for k in range(len(lines)):
                if t[2] < starts[k] < t[3]:
                    in_comment_at[k] = True
    out = []
    for i, l in enumerate(lines):
        ok_depth = depth_at_line[i] == 0 or (depth_at_line[i] == 1 and l.startswith('}'))
        if (not in_comment_at[i] and ok_depth and l and not l[0].isspace() and not l.startswith(('#', '/*'))
                and (TOPLEVEL_START.match(l) or l.startswith('}'))):
            ids = []
            for t in ctok.tokenize(l):
                if t[0] == 'id' and t[1] in gmap and t[1] not in ids:
                    ids.append(t[1])
            mk = ''
            m = None
            for n in ids:
                if PLACEHOLDER_FN.match(n) and not PLACEHOLDER_D.match(n) and re.search(r'\b%s\s*\(' % re.escape(n), l):
                    m = n
                    break
            if m:
                mk = '/*%s*/' % m
            else:
                dn = [_dmark(n) for n in ids if PLACEHOLDER_D.match(n)]
                if dn:
                    mk = '/*%s*/' % ','.join(dn)
            l = mk + l
        out.append(l)
    return '\n'.join(out)


def _comment_sub(text, cmap_re, gmap):
    if PURE_MARKER.match(text):
        return text
    return cmap_re.sub(lambda m: gmap[m.group(0)], text)


def rename_c(src, gmap, cmap_re, incmap):
    src = add_markers(src, gmap)
    out = []
    for kind, text, s, e in ctok.tokenize(src):
        if kind == 'id' and text in gmap:
            out.append(gmap[text])
        elif kind == 'pp':
            m = re.match(r'(#\s*include\s*")([^"]+)(".*)$', text, re.S)
            if m and m.group(2).lower() in incmap:
                out.append(m.group(1) + incmap[m.group(2).lower()] + m.group(3))
                continue
            sub = []
            for k2, t2, _, _ in ctok.tokenize(text, pp_as_line=False):
                if k2 == 'id' and t2 in gmap:
                    sub.append(gmap[t2])
                elif k2 == 'comment' and cmap_re:
                    sub.append(_comment_sub(t2, cmap_re, gmap))
                else:
                    sub.append(t2)
            out.append(''.join(sub))
        elif kind == 'comment' and cmap_re:
            out.append(_comment_sub(text, cmap_re, gmap))
        else:
            out.append(text)
    return ''.join(out)


_asm_split = re.compile(r"(;[^\n]*)|('[^'\n]*'|\"[^\"\n]*\")|([A-Za-z_@$?][A-Za-z0-9_@$?]*)")


def rename_asm(src, gmap):
    low = {}
    for o, n in gmap.items():
        low[o.lower()] = n
        low['_' + o.lower()] = '_' + n
    out_lines = []
    for line in src.split('\n'):
        replaced = []

        def rep(m):
            if m.group(3):
                n = low.get(m.group(3).lower())
                if n is not None:
                    replaced.append((m.start(), m.group(3)))
                    return n
            return m.group(0)
        new = _asm_split.sub(rep, line)
        # label definition at column 0 for an address-named symbol: keep the old name as comment
        if replaced and replaced[0][0] == 0:
            old = replaced[0][1].lstrip('_')
            if PLACEHOLDER_FN.match(old) or PLACEHOLDER_D.match(old):
                if ';' in new:
                    c = new.index(';')
                    new = new[:c + 1] + old + ' ' + new[c + 1:]
                else:
                    new = new.rstrip() + '\t;' + old
        out_lines.append(new)
    return '\n'.join(out_lines)


def main():
    tree = sys.argv[1]
    gmap, fmap = load_maps(sys.argv[2:])
    tech = [k for k in gmap if re.search(r'[0-9_]', k) and not re.match(r'^(_|f_[0-9a-f]{2}$)', k)]
    cmap_re = re.compile(r'\b(' + '|'.join(sorted(map(re.escape, tech), key=len, reverse=True)) + r')\b') if tech else None
    incmap = {os.path.basename(o).lower(): os.path.basename(n) for o, n in fmap.items()}
    for root, dirs, files in os.walk(tree):
        if '.git' in root.split(os.sep):
            continue
        for f in files:
            p = os.path.join(root, f)
            up = f.upper()
            if up.endswith(('.C', '.H')):
                s = open(p, encoding='latin-1').read()
                n = rename_c(s, gmap, cmap_re, incmap)
                if n != s:
                    open(p, 'w', encoding='latin-1').write(n)
            elif up.endswith(('.ASM', '.INC')):
                s = open(p, encoding='latin-1').read()
                n = rename_asm(s, gmap)
                if n != s:
                    open(p, 'w', encoding='latin-1').write(n)
    for o, n in fmap.items():
        po, pn = os.path.join(tree, o), os.path.join(tree, n)
        if os.path.exists(po) and o != n:
            r = subprocess.run(['git', 'mv', o, n], cwd=tree, capture_output=True)
            if r.returncode != 0:
                os.rename(po, pn)


if __name__ == '__main__':
    main()
