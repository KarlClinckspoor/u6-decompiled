#!/usr/bin/env python3
"""Export every NPC conversation of Ultima VI as JSON.

Reads CONVERSE.A / CONVERSE.B from the game data, decompresses each script
(Ultima VI LZW, see port/tests/u6lzw.py) and disassembles the conversation
bytecode that SRC/talkdr.c interprets (documented in docs/GAME_MECHANICS.md).
The walk is static: nothing is executed, so every answer, every branch of
every IF and every string of a random-reply array is exported, whatever the
game state.

Usage:
    python tools/dialogue_export.py [-d GAMEDATA] [-o OUT.json] [options]

    --format full     (default) per NPC: the description and the whole script
                      as "code" segments of items: text, IF / ELSE / ENDIF,
                      GOTO and its labels, SET flags, GIVEOBJ ..., inputs and
                      keyword blocks ("keywords" starts an answer).
    --format text     only the words: description, greeting and, for each
                      keyword block, the strings it can print (GOTOs followed).
    --npc N [N ...]   only these NPC numbers (default: all)
    --name SUBSTR     only NPCs whose name contains SUBSTR (case-insensitive)
    --compact         one-line JSON instead of indented
    --check           disassemble every script, list problems, write no JSON;
                      exit status 1 if there are any

Text is exported as the game stores it, so the engine's escapes are still in
it: `$P` / `#K` print a variable, `*` waits for a key, `@word` marks a keyword,
`/` and `\\` pick a plural letter.  See "Conversation" in docs/GAME_MECHANICS.md.
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'port', 'tests'))
from u6lzw import decompress  # noqa: E402

DEFAULT_DATA = os.path.join(HERE, '..', 'gamedata')

# ---------------------------------------------------------------- opcodes
OP_END_OF_FACTOR, OP_LET_VALUE = 0xA7, 0xA8
OP_ADDRESS, OP_BYTE, OP_WORD, OP_PRINT_NONE = 0xD2, 0xD3, 0xD4, 0xD5
OP_NOINDEX = 0xB1
OP_IF, OP_ENDIF, OP_ELSE = 0xA1, 0xA2, 0xA3
OP_LET, OP_GOTO, OP_PRINTSTR, OP_LEAVE = 0xA6, 0xB0, 0xB5, 0xB6
OP_VARINT, OP_VARSTR = 0xB2, 0xB3
OP_ENDRES, OP_KEY, OP_RES = 0xEE, 0xEF, 0xF6
OP_DESC, OP_MAIN = 0xF1, 0xF2
OP_ASKTOP, OP_GET, OP_GETSTR, OP_GETCHR, OP_GETINT, OP_GETDIGIT = \
    0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC
OP_SCRIPT_MARK = 0xFF
OP_NPC = 0xEB

# operators usable inside an expression: opcode -> (name, arity)
OPERATORS = {
    0x81: ('>', 2), 0x82: ('>=', 2), 0x83: ('<', 2), 0x84: ('<=', 2),
    0x85: ('!=', 2), 0x86: ('==', 2),
    0x90: ('+', 2), 0x91: ('-', 2), 0x92: ('*', 2), 0x93: ('/', 2),
    0x94: ('|', 2), 0x95: ('&', 2),
    0x9A: ('cancarry', 1), 0x9B: ('weight', 2), 0x9D: ('horsed', 1),
    0x9F: ('owns', 3), 0xA0: ('rnd', 2), 0xAB: ('tst', 2),
    0xB2: ('#', 1), 0xB3: ('$', 1), 0xB4: ('data', 2), 0xB7: ('strsearch', 2),
    0xBB: ('test_obj', 2), 0xBC: ('stub_bc', 2), 0xBD: ('stub_bd', 2),
    0xC0: ('select_object', 0), 0xC1: ('owner', 1), 0xC2: ('objtype', 1),
    0xC3: ('valsearch', 2), 0xC6: ('isinparty', 1), 0xC7: ('whosgot', 2),
    0xCA: ('join', 1), 0xCC: ('leaveparty', 1), 0xD7: ('isonscreen', 1),
    0xDA: ('wounded', 1), 0xDC: ('poisoned', 1), 0xDD: ('partymember', 2),
    0xE0: ('addexp', 2), 0xE1: ('addlvl', 2), 0xE2: ('addstr', 2),
    0xE3: ('addint', 2), 0xE4: ('adddex', 2),
}
INFIX = {'>', '>=', '<', '<=', '!=', '==', '+', '-', '*', '/', '|', '&'}

# statements: opcode -> (name, number of expression operands)
STATEMENTS = {
    0xA4: ('set', 2), 0xA5: ('clr', 2),
    0xB9: ('giveobj', 4), 0xBA: ('takeobj', 4),
    0xBE: ('show_inventory', 1), 0xBF: ('show_converse', 1),
    0xC4: ('addkarma', 1), 0xC5: ('subkarma', 1),
    0xC8: ('moveobj', 2), 0xC9: ('transferobj', 4),
    0xCD: ('setmode', 2), 0xD0: ('delay', 1), 0xD6: ('resurrect', 1),
    0xD8: ('setname_y', 1), 0xD9: ('heal', 1), 0xDB: ('cure', 1),
    0xDF: ('setname_y_you', 1), 0x9C: ('gethorse', 1),
}
NO_OPERAND = {0x9E: 'rest', 0xCB: 'wait'}


class ScriptError(Exception):
    pass


def hexaddr(a):
    return '@%04x' % a


class Script:
    """One decompressed script, disassembled by following control flow.

    The compiler puts word arrays and string tables inline in the code (right
    after a GOTO, typically), so a linear scan would read data as code.
    Instead the walk starts at the main section and follows fall-through,
    keyword blocks and GOTO targets; bytes that are never reached are data.
    """

    def __init__(self, data):
        self.d = data
        self.p = 0
        self.instr = {}         # address -> (item, length); text is 1 byte/char
        self.labels = set()     # GOTO targets
        self.pending = []       # variable-index string arrays, resolved last
        self.data = set()       # addresses of strings read as data
        self.todo = []

    # ---- low level readers (cursor is self.p)
    def u8(self):
        if self.p >= len(self.d):
            raise ScriptError('ran off the end of the script')
        b = self.d[self.p]
        self.p += 1
        return b

    def u32(self):
        if self.p + 4 > len(self.d):
            raise ScriptError('ran off the end of the script')
        v = struct.unpack_from('<I', self.d, self.p)[0]
        self.p += 4
        return v

    def cstr(self, addr):
        end = self.d.find(b'\x00', addr)
        if addr >= len(self.d) or end < 0:
            raise ScriptError('string address 0x%x out of range' % addr)
        self.data.add(addr)
        return self.d[addr:end].decode('latin-1')

    # ---- expressions (postfix, ended by A7 / A8)
    def factor(self):
        toks = []
        while True:
            b = self.u8()
            if b in (OP_END_OF_FACTOR, OP_LET_VALUE):
                break
            if b == OP_ADDRESS:
                toks.append(('addr', self.u32()))
            elif b == OP_BYTE:
                toks.append(('num', self.u8()))
            elif b == OP_WORD:
                lo = self.u8()
                toks.append(('num', lo | self.u8() << 8))
            elif b in OPERATORS:
                toks.append(('op', b))
            else:
                toks.append(('num', b))      # raw small constant (0xEB = npc)
        return self.build(toks)

    @staticmethod
    def build(toks):
        st = []
        for kind, v in toks:
            if kind != 'op':
                st.append((kind, v))
                continue
            name, arity = OPERATORS[v]
            if name == 'data' and len(st) == 1:
                # `@array #n DATA` as the index of a string array: the
                # engine's stack underflows here and the index survives
                # unchanged, so DATA acts as a no-op.
                continue
            if len(st) < arity:
                raise ScriptError('operator %s lacks operands' % name)
            args = st[len(st) - arity:]
            del st[len(st) - arity:]
            st.append((name, args))
        if len(st) != 1:
            raise ScriptError('expression leaves %d values' % len(st))
        return st[0]

    def show(self, e):
        kind, v = e
        if kind == 'num':
            return 'npc' if v == OP_NPC else str(v)
        if kind == 'addr':
            return hexaddr(v)
        if kind in ('#', '$'):
            return kind + self.var(v[0])
        if kind in INFIX:
            return '(%s %s %s)' % (self.show(v[0]), kind, self.show(v[1]))
        return '%s(%s)' % (kind, ', '.join(self.show(a) for a in v))

    @staticmethod
    def var(e):
        n = e[1] if e[0] == 'num' else 0
        return str(n) if n < 10 else chr(n + 0x37)

    @staticmethod
    def index_range(e):
        if e[0] == 'num':
            return e[1], e[1]
        if e[0] == 'rnd' and all(a[0] == 'num' for a in e[1]):
            return e[1][0][1], e[1][1][1]
        return None, None

    # ---- string operands (PRINTSTR / LET $n := ...), after the D2 byte
    def addr_operand(self):
        addr = self.u32()
        if self.u8() == OP_NOINDEX:
            return {'address': hexaddr(addr), 'strings': [self.cstr(addr)]}
        self.p -= 1
        idx = self.factor()
        lo, hi = self.index_range(idx)
        r = {'address': hexaddr(addr), 'index': self.show(idx)}
        if lo is None:                  # variable index: the table's size is
            r['choice'] = 'variable'    # known only once all code is known
            self.pending.append((r, addr))
            return r
        r['choice'] = 'random' if lo != hi else 'fixed'
        out, a = [], addr
        for i in range(hi + 1):
            s = self.cstr(a)
            if i >= lo:
                out.append(s)
            a += len(s) + 1
        r['strings'] = out
        return r

    # ---- one instruction at self.p -> item
    def decode(self):
        b = self.u8()
        if b < 0x80:
            if b == 0:
                raise ScriptError('NUL where a statement was expected')
            return {'ch': chr(b)}
        if b == OP_IF:
            return {'if': self.show(self.factor())}
        if b == OP_ELSE:
            return {'else': True}
        if b == OP_ENDIF:
            return {'endif': True}
        if b == OP_GOTO:
            t = self.u32()
            self.labels.add(t)
            return {'goto': hexaddr(t)}
        if b == OP_LEAVE:
            return {'leave': True}
        if b in NO_OPERAND:
            return {'op': NO_OPERAND[b]}
        if b in STATEMENTS:
            name, n = STATEMENTS[b]
            return {'op': name,
                    'args': [self.show(self.factor()) for _ in range(n)]}
        if b == OP_PRINTSTR:
            c = self.u8()
            if c == OP_ADDRESS:
                return {'print_string': self.addr_operand()}
            if c == OP_PRINT_NONE:
                return {'print_nothing': True}
            idx = self.u8()                 # <D3> <n> $ <A7>
            if self.u8() != OP_VARSTR:
                raise ScriptError('bad PRINTSTR operand')
            self.u8()
            return {'print_var': '$' + self.var(('num', idx))}
        if b == OP_LET:
            return self.let()
        if b in (OP_END_OF_FACTOR, OP_LET_VALUE):
            return {'nop': True}
        if b == OP_ENDRES:
            return {'end_answer': True}
        if b == OP_KEY:
            kw = bytearray()
            while True:
                c = self.u8()
                if c == OP_RES:
                    break
                kw.append(c)
            return {'keywords': kw.decode('latin-1').split(',')}
        if b == OP_ASKTOP:
            return {'input': 'line'}
        if b in (OP_GETSTR, OP_GETINT, OP_GETDIGIT):
            v = self.u8()
            kind = self.u8()
            return {'input': {OP_GETSTR: 'string', OP_GETINT: 'number',
                              OP_GETDIGIT: 'digit'}[b],
                    'into': ('$' if kind == OP_VARSTR else '#') +
                            self.var(('num', v))}
        if b == OP_GETCHR:
            return {'input': 'key'}
        if b == OP_GET:
            keys = bytearray()
            while self.p < len(self.d) and self.d[self.p] != OP_KEY:
                keys.append(self.u8())
            return {'input': 'one of keys', 'keys': keys.decode('latin-1')}
        raise ScriptError('unknown opcode 0x%02x at 0x%x' % (b, self.p - 1))

    def let(self):
        v = self.u8()
        if v == OP_ADDRESS:                         # array word := expr
            a = self.u32()
            if self.u8() == OP_NOINDEX:
                idx, tail = None, self.u8()
            else:
                self.p -= 1
                idx, tail = self.factor(), self.d[self.p - 1]
            if tail != OP_LET_VALUE:
                raise ScriptError('bad LET destination')
            dest = hexaddr(a) + ('' if idx is None else '[%s]' % self.show(idx))
            return {'let': '%s := %s' % (dest, self.show(self.factor()))}
        kind = self.u8()
        if self.u8() != OP_LET_VALUE:
            raise ScriptError('bad LET')
        name = self.var(('num', v))
        if kind == OP_VARINT:
            return {'let': '#%s := %s' % (name, self.show(self.factor()))}
        if kind != OP_VARSTR:
            raise ScriptError('bad LET target')
        c = self.u8()
        if c == OP_ADDRESS:
            r = self.addr_operand()
            r['var'] = '$' + name
            return {'set_string': r}
        if self.u8() != OP_VARSTR:
            raise ScriptError('bad LET source')
        return {'let': '$%s := $%s' % (name, self.var(('num', c)))}

    # ---- control-flow walk
    def walk(self, start):
        p = start
        blocks = []         # open IFs: [has_else, true_branch_ended, else_ended]
        while 0 <= p < len(self.d) and p not in self.instr:
            if self.d[p] == OP_SCRIPT_MARK:
                self.instr[p] = ({'end_of_script': True}, 1)
                return
            self.p = p
            item = self.decode()
            self.instr[p] = (item, self.p - p)
            p = self.p
            ends = False            # does the flow end after this item?
            if 'if' in item:
                blocks.append([False, False, False])
            elif 'else' in item:
                if blocks:
                    blocks[-1][0] = True
            elif 'endif' in item:
                if blocks:
                    has_else, t_end, e_end = blocks.pop()
                    ends = has_else and t_end and e_end and not blocks
            elif 'goto' in item or 'leave' in item:
                if 'goto' in item:
                    self.todo.append(int(item['goto'][1:], 16))
                if blocks:
                    blocks[-1][2 if blocks[-1][0] else 1] = True
                else:
                    ends = True
            # A jump or LEAVE that ends the flow leaves data or a jump
            # target behind, unless what follows is the next keyword block
            # of the same prompt (which the engine finds by scanning).
            if ends and not self.is_code(p):
                return

    def is_code(self, p):
        """After an unconditional GOTO / LEAVE: is the next block code the
        engine reaches by scanning (more keyword blocks, the next prompt)
        rather than inline data?  Decode ahead without recording anything;
        data soon hits a NUL or an unknown opcode."""
        d = self.d
        if p >= len(d) or d[p] not in (OP_KEY, OP_ENDRES):
            return False
        saved = (set(self.labels), len(self.pending), set(self.data), self.p)
        ok = False
        try:
            while p < len(d):
                if d[p] == OP_SCRIPT_MARK:
                    ok = True
                    break
                self.p = p
                item = self.decode()
                p = self.p
                if 'input' in item or 'goto' in item or 'leave' in item:
                    ok = True
                    break
        except ScriptError:
            pass
        self.labels, self.data, self.p = saved[0], saved[2], saved[3]
        del self.pending[saved[1]:]
        return ok

    def parse(self):
        d = self.d
        if not d or d[0] != OP_SCRIPT_MARK:
            raise ScriptError('no script mark')
        self.p = 2
        name = bytearray()
        while self.p < len(d) and d[self.p] < 0x80:
            name.append(d[self.p])
            self.p += 1
        if self.p < len(d) and d[self.p] == 0xF3:      # marker before F1
            self.p += 1
        if self.p >= len(d) or d[self.p] != OP_DESC:
            raise ScriptError('no description')
        self.p += 1
        desc_start = self.p
        # the description runs up to the first byte >= 0xF0 (an 0xF3 section
        # marker, or OP_MAIN); the engine then skips ahead to OP_MAIN
        while self.p < len(d) and d[self.p] < 0xF0:
            at = self.p
            self.instr[at] = (self.decode(), self.p - at)
        desc_end = self.p
        main = d.find(bytes([OP_MAIN]), desc_end)
        if main < 0:
            raise ScriptError('no main section')
        self.main = main + 1
        self.todo = [self.main]
        walked = set()
        while self.todo:
            s = self.todo.pop()
            if s not in walked:
                walked.add(s)
                self.walk(s)
        for r, addr in self.pending:        # variable-index string tables
            out, a = [], addr
            while a < len(d) and a not in self.instr and d[a] >= 0x20:
                s = self.cstr(a)
                out.append(s)
                a += len(s) + 1
            r['strings'] = out
        segments, cur, prev = [], None, None
        for a in sorted(x for x in self.instr if x >= self.main):
            if a != prev:
                cur = {'at': hexaddr(a), 'items': []}
                segments.append(cur)
            if a in self.labels:
                cur['items'].append({'label': hexaddr(a)})
            item, n = self.instr[a]
            cur['items'].append(item)
            prev = a + n
        for s in segments:
            s['items'] = merge_text(s['items'])
        return {'npc': d[1], 'name': name.decode('latin-1'),
                'description': merge_text(
                    [self.instr[a][0] for a in sorted(self.instr)
                     if desc_start <= a < desc_end]),
                'code': segments}

    # ---- the words only
    def follow(self, addr, seen, depth=0):
        """Every string reachable from `addr` up to the next prompt, keyword
        block or end of conversation, GOTOs followed (once per target)."""
        out, buf = [], ''
        while addr in self.instr:
            item, n = self.instr[addr]
            addr += n
            if 'ch' in item:
                buf += item['ch']
                continue
            if buf:
                out.append(buf)
                buf = ''
            if 'print_string' in item:
                out.extend(item['print_string']['strings'])
            elif 'set_string' in item:
                out.extend(item['set_string']['strings'])
            elif 'if' in item:
                depth += 1
            elif 'endif' in item:
                depth = max(0, depth - 1)
            elif 'goto' in item:
                t = int(item['goto'][1:], 16)
                if t not in seen:
                    seen.add(t)
                    out.extend(self.follow(t, seen, 0))
                if depth == 0:
                    break
            elif 'leave' in item:
                if depth == 0:
                    break
            elif 'keywords' in item or 'input' in item or \
                    'end_answer' in item or 'end_of_script' in item:
                break
        if buf:
            out.append(buf)
        return out


def merge_text(items):
    out = []
    for it in items:
        if 'ch' in it:
            if out and 'text' in out[-1]:
                out[-1]['text'] += it['ch']
            else:
                out.append({'text': it['ch']})
        else:
            out.append(it)
    return out


def text_view(s, script):
    """Words only: description, greeting, and per keyword block the strings
    it can print."""
    topics = []
    for a in sorted(script.instr):
        item, n = script.instr[a]
        if 'keywords' in item:
            topics.append({'keywords': item['keywords'],
                           'text': script.follow(a + n, {a})})
    desc = [it['text'] for it in s['description'] if 'text' in it]
    return {'npc': s['npc'], 'name': s['name'], 'file': s['file'],
            'index': s['index'], 'description': desc,
            'greeting': script.follow(script.main, {script.main}),
            'topics': topics}


# ------------------------------------------------------------ file handling
def load_scripts(path):
    """Yield (index, raw script bytes) for every non-empty entry of a
    CONVERSE file (4-byte offset table, then size + LZW data per entry)."""
    d = open(path, 'rb').read()
    # the table ends where the first script starts: the first non-zero offset
    first = next(v for v in (struct.unpack_from('<I', d, i)[0]
                             for i in range(0, len(d) - 3, 4)) if v)
    for i in range(first // 4):
        off = struct.unpack_from('<I', d, i * 4)[0]
        if not off:
            continue
        size = struct.unpack_from('<I', d, off)[0]
        if size and size < 0x2800:
            raw, _ = decompress(d[off:])
        else:                                   # stored raw (e.g. NPC 125)
            raw = d[off + 4:off + 4 + 0x2800]
        yield i, raw


def export(data_dir, errors, fmt):
    result = []
    for letter in 'AB':
        path = os.path.join(data_dir, 'CONVERSE.' + letter)
        for index, raw in load_scripts(path):
            sc = Script(raw)
            try:
                s = sc.parse()
            except ScriptError as e:
                errors.append('CONVERSE.%s #%d: %s' % (letter, index, e))
                continue
            s = {'npc': s['npc'], 'name': s['name'], 'file': 'CONVERSE.' + letter,
                 'index': index, **{k: v for k, v in s.items()
                                    if k not in ('npc', 'name')}}
            result.append(text_view(s, sc) if fmt == 'text' else s)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('-d', '--data', default=DEFAULT_DATA,
                    help='folder with CONVERSE.A/B (default: ../gamedata)')
    ap.add_argument('-o', '--output', help='write here instead of stdout')
    ap.add_argument('--format', choices=('full', 'text'), default='full')
    ap.add_argument('--npc', type=int, nargs='+', help='only these NPC numbers')
    ap.add_argument('--name', help='only NPCs whose name contains this')
    ap.add_argument('--compact', action='store_true')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    errors = []
    try:
        scripts = export(a.data, errors, a.format)
    except (OSError, StopIteration) as e:
        sys.exit('error: cannot read CONVERSE.A/B in %s (%s)' % (a.data, e))
    if a.check:
        for e in errors:
            print(e)
        print('%d scripts ok, %d with problems' % (len(scripts), len(errors)))
        sys.exit(1 if errors else 0)
    for e in errors:
        print('warning: skipped ' + e, file=sys.stderr)
    if a.npc:
        scripts = [s for s in scripts if s['npc'] in a.npc]
    if a.name:
        scripts = [s for s in scripts if a.name.lower() in s['name'].lower()]
    out = json.dumps(scripts, ensure_ascii=False,
                     indent=None if a.compact else 2,
                     separators=(',', ':') if a.compact else None)
    if a.output:
        with open(a.output, 'w', encoding='utf-8', newline='\n') as f:
            f.write(out + '\n')
        print('%d scripts -> %s' % (len(scripts), a.output), file=sys.stderr)
    else:
        sys.stdout.reconfigure(encoding='utf-8')
        print(out)
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
