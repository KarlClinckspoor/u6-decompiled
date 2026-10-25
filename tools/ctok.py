"""Minimal C tokenizer (Turbo C 2.0 dialect: no // comments).

tokenize(src) -> list of (kind, text, start, end)
kinds: 'ws', 'nl', 'comment', 'pp' (whole preprocessor line incl. continuations),
       'str', 'chr', 'num', 'id', 'punct'
"""
import re

_PUNCT = sorted("""
>>= <<= ... -> ++ -- << >> <= >= == != && || *= /= %= += -= &= ^= |= ##
{ } [ ] ( ) ; : , . ? ~ ! + - * / % < > = & ^ | #
""".split(), key=len, reverse=True)

_id_re = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
_num_re = re.compile(r'\.?[0-9](?:[eEpP][+-]|[A-Za-z0-9_.])*')


def tokenize(src, pp_as_line=True):
    toks = []
    i, n = 0, len(src)
    at_line_start = True
    while i < n:
        c = src[i]
        if c == '\n':
            toks.append(('nl', c, i, i + 1)); i += 1; at_line_start = True; continue
        if c in ' \t\r\f\v':
            j = i
            while j < n and src[j] in ' \t\r\f\v':
                j += 1
            toks.append(('ws', src[i:j], i, j)); i = j; continue
        if c == '/' and src.startswith('/*', i):
            j = src.find('*/', i + 2)
            if j < 0:
                raise ValueError('unterminated comment at %d' % i)
            toks.append(('comment', src[i:j + 2], i, j + 2)); i = j + 2; continue
        if c == '#' and at_line_start and pp_as_line:
            # preprocessor line: up to newline not preceded by backslash, comments may span
            j = i
            while j < n:
                if src[j] == '\n':
                    k = j - 1
                    while k > i and src[k] == '\r':
                        k -= 1
                    if src[k] == '\\':
                        j += 1; continue
                    break
                if src.startswith('/*', j):
                    e = src.find('*/', j + 2)
                    j = e + 2; continue
                j += 1
            toks.append(('pp', src[i:j], i, j)); i = j; continue
        at_line_start = False
        if c == '"' or c == "'":
            j = i + 1
            while j < n and src[j] != c:
                if src[j] == '\\':
                    j += 1
                j += 1
            toks.append(('str' if c == '"' else 'chr', src[i:j + 1], i, j + 1)); i = j + 1; continue
        m = _id_re.match(src, i)
        if m:
            toks.append(('id', m.group(), i, m.end())); i = m.end(); continue
        m = _num_re.match(src, i)
        if m:
            toks.append(('num', m.group(), i, m.end())); i = m.end(); continue
        for p in _PUNCT:
            if src.startswith(p, i):
                toks.append(('punct', p, i, i + len(p))); i += len(p); break
        else:
            toks.append(('punct', c, i, i + 1)); i += 1
    return toks


def sig_tokens(src):
    """significant tokens of already-preprocessed text"""
    return [t for t in tokenize(src, pp_as_line=False) if t[0] not in ('ws', 'nl', 'comment')]


def int_value(tok_kind, text):
    """Return (value, ctype) under 16-bit Turbo C rules, or None."""
    if tok_kind == 'chr':
        body = text[1:-1]
        if body.startswith('\\'):
            esc = body[1:]
            table = {'n': 10, 't': 9, 'r': 13, '0': 0, '\\': 92, "'": 39, '"': 34, 'a': 7, 'b': 8, 'f': 12, 'v': 11}
            if esc and esc[0] == 'x':
                v = int(esc[1:], 16)
            elif esc and esc[0] in '01234567':
                v = int(esc, 8)
            elif esc in table:
                v = table[esc]
            else:
                v = ord(esc[0])
        else:
            if len(body) != 1:
                return None
            v = ord(body)
        if v >= 128:  # char is signed by default in TC
            v -= 256
        return (v, 'int')
    if tok_kind != 'num':
        return None
    t = text
    suf = ''
    while t and t[-1] in 'uUlL':
        suf = t[-1].lower() + suf; t = t[:-1]
    try:
        if t.lower().startswith('0x'):
            v = int(t, 16); dec = False
        elif len(t) > 1 and t.startswith('0') and t.isdigit():
            v = int(t, 8); dec = False
        elif t.isdigit():
            v = int(t); dec = True
        else:
            return None
    except ValueError:
        return None
    u = 'u' in suf
    l = 'l' in suf
    if dec:
        cands = (['unsigned long'] if u and l else ['unsigned', 'unsigned long'] if u else
                 ['long', 'unsigned long'] if l else ['int', 'long', 'unsigned long'])
    else:
        cands = (['unsigned long'] if u and l else ['unsigned', 'unsigned long'] if u else
                 ['long', 'unsigned long'] if l else ['int', 'unsigned', 'long', 'unsigned long'])
    lim = {'int': 0x7fff, 'unsigned': 0xffff, 'long': 0x7fffffff, 'unsigned long': 0xffffffff}
    for c in cands:
        if v <= lim[c]:
            return (v, c)
    return (v, 'overflow')
