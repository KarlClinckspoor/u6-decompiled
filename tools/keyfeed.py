#!/usr/bin/env python3
"""KEYFEED.COM: a small DOS TSR that types a fixed key script into a program, counted in the
program's own keyboard polls instead of in time.

Why. Keys typed in real time (DOSBox-X's AUTOTYPE, js-dos) reach the program after a number of
frames that changes from run to run with the host's speed. Ultima VI's programs call rand() once
a frame in places, so the run then differs:

- U.EXE's character creation shuffles the gypsy's questions with rand(), seeded by
  srand(time(0)), and its animations call rand() every frame (chargen.c's DrawPattern, from the
  first answer on), so the second round's order depends on how many frames passed before each
  key. Mode `dos` (int 21h) makes it fixed, and gives the program a fixed date and time too.
- GAME.EXE reads keys through the BIOS (KBD.ASM's KBD_GetKey: int 16h function 1, then 0).
  Mode `bios` (int 16h) types a session into it. Every recording of a script is then the same
  run, whatever the host's speed, and it can type what AUTOTYPE cannot: Ctrl+S, the game's save.

How. A key is offered once the program has asked "is there a key?" GAP times since the last key
was read, and taken by the next read:

- mode dos: int 21h function 0Bh (Turbo C's kbhit: `mov ah,0Bh; int 21h; cbw`) says "no key"
  until it has been called GAP times, then "a key"; functions 07h and 08h (getch:
  `mov ax,0700h; int 21h`) give the key's low byte at once; functions 2Ah and 2Ch (getdate,
  gettime, so time()) give a fixed date and time;
- mode bios: int 16h functions 01h and 11h set ZF (no key) until called GAP times, then clear it
  with AX the key; functions 00h and 10h give AX and take it; int 21h functions 2Ah and 2Ch
  give the fixed date and time (GAME's OSI_timeSeed, RAND.ASM, makes the random seed of the
  time of day: fixed, a recording of a script is the same file every time);
- every other call, and every read once the script is used up, goes on to the previous handler.

So every run makes the same calls in the same order and reads the same keys at the same poll.
The program itself is not changed. KEYFEED.COM is assembled here, byte by byte, from the listing
in code() (no assembler needed): `--list` prints it.

    python3 tools/keyfeed.py OUT.COM --mode dos|bios --keys "esc/5 c/40 ..."   (parse_keys)
"""
import argparse, struct, sys

ORG = 0x100

# scan codes (US layout) of the keys a script may name, by their character
_ROWS = [(0x02, '1234567890-='), (0x10, 'qwertyuiop[]'), (0x1e, "asdfghjkl;'`"), (0x2b, '\\zxcvbnm,./')]
_SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6', '&': '7', '*': '8', '(': '9',
            ')': '0', '_': '-', '+': '=', '{': '[', '}': ']', ':': ';', '"': "'", '~': '`', '|': '\\',
            '<': ',', '>': '.', '?': '/'}
SCAN = {}
for base, chars in _ROWS:
    for i, c in enumerate(chars):
        SCAN[c] = base + i
SCAN[' '] = 0x39

# named keys: the BIOS's AX for each (scan code << 8 | ASCII); the arrows, the function keys and
# the like have no ASCII (0), and int 21h's getch gives them as two bytes, 0 then the scan code
NAMED = {'esc': 0x011b, 'enter': 0x1c0d, 'space': 0x3920, 'backspace': 0x0e08, 'tab': 0x0f09,
         'left': 0x4b00, 'right': 0x4d00, 'up': 0x4800, 'down': 0x5000,
         'home': 0x4700, 'end': 0x4f00, 'pageup': 0x4900, 'pagedown': 0x5100, 'insert': 0x5200, 'delete': 0x5300}
for _i in range(10):
    NAMED['f%d' % (_i + 1)] = (0x3b + _i) << 8


def key_ax(tok):
    """the BIOS's AX for one key: a NAMED name, one character, ctrl+X / alt+X for a letter X, or
    altnum+NNN, the character NNN typed with Alt and the keypad's digits (scan code 0: GAME's
    debug commands, altnum+214 its "go to coordinate")"""
    t = tok.lower()
    if t in NAMED:
        return NAMED[t]
    if t.startswith('altnum+') and t[7:].isdigit() and 0 < int(t[7:]) < 256:
        return int(t[7:])
    if t.startswith('ctrl+') and len(t) == 6 and t[5].isalpha():
        return SCAN[t[5]] << 8 | (ord(t[5]) - 0x60)
    if t.startswith('alt+') and len(t) == 5 and t[4] in SCAN:
        return SCAN[t[4]] << 8
    if len(tok) == 1:
        c = tok
        base = c.lower() if c.isalpha() else _SHIFTED.get(c, c)
        if base in SCAN:
            return SCAN[base] << 8 | ord(c)
    sys.exit('keyfeed: unknown key %r' % tok)


def parse_keys(text, gap=30, mode='dos'):
    """'KEY[/GAP] ...' to [(gap, value)]. KEY is a name of NAMED (esc, enter, space, left, f1,
    ...), one character as typed (a, A, 1, ...), ctrl+X or alt+X for a letter, or t:TEXT (each
    character of TEXT, GAP polls apart). GAP is the number of polls that say "no key" before this
    key is offered (default: the script's gap). value is the BIOS's AX in mode bios; in mode dos
    the byte getch gives, and a key without ASCII is two entries, 0 then its scan code."""
    toks = []
    for tok in text.split():
        g = gap
        if '/' in tok[1:]:
            tok, g = tok.rsplit('/', 1)
            g = int(g)
        if tok.startswith('t:'):
            toks += [(g, key_ax(ch)) for ch in tok[2:]]
        else:
            toks.append((g, key_ax(tok)))
    out = []
    for g, ax in toks:
        if not (1 if mode == 'bios' else 0) <= g < 0xffff:
            # (in mode bios a gap of 0 would let KBD_GetKey's "keep the last key" loop read two
            # keys in one call and drop the first)
            sys.exit('keyfeed: gap %d out of range' % g)
        if mode == 'bios':
            out.append((g, ax))
        elif ax & 0xff:
            out.append((g, ax & 0xff))
        else:
            out += [(g, 0), (0, ax >> 8)]
    return out


def code(keys, date=(1990, 1, 1, 1), clock=(12, 0, 0), mode='dos'):
    """the program: [(offset, bytes, text)] and the whole image. date = (year, month, day, day of
    the week), clock = (hours, minutes, seconds), both for mode dos"""
    year, month, day, dow = date
    hh, mm, ss = clock
    vec = 0x21 if mode == 'dos' else 0x16
    items = []        # (label or None, bytes or f(labels, own offset) -> bytes, text)

    def at(name):
        items.append((name, b'', name + ':'))

    def op(b, text):
        items.append((None, b, text))

    def w(x):
        return struct.pack('<H', x & 0xffff)

    def rel8(frm, to):
        d = to - frm
        assert -128 <= d <= 127, (frm, to)
        return bytes([d & 0xff])

    def jcc(opc, name, text):
        op(lambda L, o: bytes([opc]) + rel8(o + 2, L[name]), text)

    def ref(prefix, name, suffix, text, add=0):
        op(lambda L, o: bytes(prefix) + w(L[name] + add) + bytes(suffix), text)

    def cmp_ah(fn, lab):
        op(lambda L, o: bytes([0x80, 0xfc, fn, 0x74]) + rel8(o + 5, L[lab]), 'cmp ah,%02Xh / je %s' % (fn, lab))

    def entry(text_end):
        """si = idx, bx = the entry's gap; to text_end if the script is used up"""
        op(b'\x56', 'push si')
        op(b'\x53', 'push bx')
        ref([0x2e, 0x8b, 0x36], 'idx', [], 'mov si,cs:[idx]')
        ref([0x2e, 0x8b, 0x9c], 'script', [], "mov bx,cs:[si+script]   ; the entry's gap")
        op(b'\x83\xfb\xff', 'cmp bx,0FFFFh           ; the end of the script')
        jcc(0x74, text_end, 'je ' + text_end)

    def take():
        ref([0x2e, 0x83, 0x06], 'idx', [4], 'add word ptr cs:[idx],4  ; taken')
        ref([0x2e, 0xc7, 0x06], 'cnt', [0, 0], 'mov word ptr cs:[cnt],0')

    op(lambda L, o: b'\xe9' + w(L['init'] - (o + 3)), 'jmp init')
    at('old'); op(b'\0\0\0\0', 'dd 0                    ; the previous int %02Xh' % vec)
    at('idx'); op(w(0), 'dw 0                    ; offset of the next entry in script')
    at('cnt'); op(w(0), 'dw 0                    ; polls since the last key was read')
    if mode == 'bios':
        at('old21'); op(b'\0\0\0\0', 'dd 0                    ; the previous int 21h')

    at('handler')
    if mode == 'dos':
        for fn, lab in ((0x0b, 'poll'), (0x07, 'read'), (0x08, 'read'), (0x2a, 'gdate'), (0x2c, 'gtime')):
            cmp_ah(fn, lab)
    else:
        for fn, lab in ((0x01, 'poll'), (0x11, 'poll'), (0x00, 'read'), (0x10, 'read')):
            cmp_ah(fn, lab)
    at('chain')
    ref([0x2e, 0xff, 0x2e], 'old', [], 'jmp far cs:[old]')

    at('poll')
    if mode == 'dos':
        entry('pno')
        ref([0x2e, 0x39, 0x1e], 'cnt', [], 'cmp cs:[cnt],bx')
        jcc(0x73, 'pyes', 'jae pyes')
        ref([0x2e, 0xff, 0x06], 'cnt', [], 'inc word ptr cs:[cnt]')
        at('pno')
        op(b'\xb0\x00', 'mov al,0                ; no key')
        jcc(0xeb, 'pout', 'jmp short pout')
        at('pyes')
        op(b'\xb0\xff', 'mov al,0FFh             ; a key')
        at('pout')
        op(b'\x5b', 'pop bx')
        op(b'\x5e', 'pop si')
        op(b'\xcf', 'iret')
    else:
        entry('pend')
        ref([0x2e, 0x39, 0x1e], 'cnt', [], 'cmp cs:[cnt],bx')
        jcc(0x73, 'pyes', 'jae pyes')
        ref([0x2e, 0xff, 0x06], 'cnt', [], 'inc word ptr cs:[cnt]')
        op(b'\x5b', 'pop bx')
        op(b'\x5e', 'pop si')
        op(b'\x55', 'push bp')
        op(b'\x8b\xec', 'mov bp,sp')
        op(b'\x83\x4e\x06\x40', 'or word ptr [bp+6],40h  ; ZF in the caller\'s flags: no key')
        op(b'\x5d', 'pop bp')
        op(b'\xcf', 'iret')
        at('pyes')
        ref([0x2e, 0x8b, 0x84], 'script', [], 'mov ax,cs:[si+script+2] ; the key', add=2)
        op(b'\x5b', 'pop bx')
        op(b'\x5e', 'pop si')
        op(b'\x55', 'push bp')
        op(b'\x8b\xec', 'mov bp,sp')
        op(b'\x83\x66\x06\xbf', 'and word ptr [bp+6],0FFBFh ; ZF clear: a key')
        op(b'\x5d', 'pop bp')
        op(b'\xcf', 'iret')
        at('pend')
        op(b'\x5b', 'pop bx')
        op(b'\x5e', 'pop si')
        op(lambda L, o: b'\xeb' + rel8(o + 2, L['chain']), 'jmp short chain        ; used up: the BIOS')

    at('read')
    entry('rend')
    if mode == 'dos':
        ref([0x2e, 0x8a, 0x84], 'script', [], 'mov al,cs:[si+script+2] ; the key', add=2)
    else:
        ref([0x2e, 0x8b, 0x84], 'script', [], 'mov ax,cs:[si+script+2] ; the key', add=2)
    take()
    op(b'\x5b', 'pop bx')
    op(b'\x5e', 'pop si')
    op(b'\xcf', 'iret')
    at('rend')
    op(b'\x5b', 'pop bx')
    op(b'\x5e', 'pop si')
    op(lambda L, o: b'\xeb' + rel8(o + 2, L['chain']), 'jmp short chain        ; used up: the previous handler')

    if mode == 'bios':
        at('clock')                  # int 21h in mode bios: only the date and the time
        for fn, lab in ((0x2a, 'gdate'), (0x2c, 'gtime')):
            cmp_ah(fn, lab)
        ref([0x2e, 0xff, 0x2e], 'old21', [], 'jmp far cs:[old21]')
    if True:                         # both modes: the fixed date and time
        at('gdate')
        op(b'\xb9' + w(year), 'mov cx,%d' % year)
        op(b'\xba' + w(month << 8 | day), 'mov dx,%04Xh            ; month %d, day %d' % (month << 8 | day, month, day))
        op(b'\xb0' + bytes([dow]), 'mov al,%d                ; day of the week' % dow)
        op(b'\xcf', 'iret')
        at('gtime')
        op(b'\xb9' + w(hh << 8 | mm), 'mov cx,%04Xh            ; %02d:%02d' % (hh << 8 | mm, hh, mm))
        op(b'\xba' + w(ss << 8), 'mov dx,%04Xh            ; %02d.00 s' % (ss << 8, ss))
        op(b'\xcf', 'iret')

    at('script')
    for g, v in keys:
        op(w(g) + w(v), 'dw %5d, %04Xh' % (g, v))
    op(w(0xffff) + w(0), 'dw 0FFFFh, 0              ; the end')

    at('init')                       # kept resident with the rest: it is small
    op(b'\xb8' + w(0x3500 | vec), 'mov ax,35%02Xh' % vec)
    op(b'\xcd\x21', 'int 21h')
    ref([0x89, 0x1e], 'old', [], 'mov [old],bx')
    ref([0x8c, 0x06], 'old', [], 'mov [old+2],es', add=2)
    ref([0xba], 'handler', [], 'mov dx,offset handler')
    op(b'\xb8' + w(0x2500 | vec), 'mov ax,25%02Xh' % vec)
    op(b'\xcd\x21', 'int 21h')
    if mode == 'bios':
        op(b'\xb8\x21\x35', 'mov ax,3521h')
        op(b'\xcd\x21', 'int 21h')
        ref([0x89, 0x1e], 'old21', [], 'mov [old21],bx')
        ref([0x8c, 0x06], 'old21', [], 'mov [old21+2],es', add=2)
        ref([0xba], 'clock', [], 'mov dx,offset clock')
        op(b'\xb8\x21\x25', 'mov ax,2521h')
        op(b'\xcd\x21', 'int 21h')
    op(lambda L, o: b'\xba' + w((L['end'] + 15) // 16), 'mov dx,(end+15)/16      ; paragraphs kept, from the PSP')
    op(b'\xb8\x00\x31', 'mov ax,3100h            ; terminate and stay resident')
    op(b'\xcd\x21', 'int 21h')
    at('end')

    # two passes: every instruction's size is fixed, so the first lays out the labels
    names = [n for n, _, _ in items if n]
    L = {}
    o = ORG
    for name, b, _ in items:
        if name:
            L[name] = o
        o += len(b({k: ORG for k in names}, ORG)) if callable(b) else len(b)
    listing, img, o = [], b'', ORG
    for name, b, text in items:
        bb = b(L, o) if callable(b) else b
        listing.append((o, bb, text))
        img += bb
        o += len(bb)
    assert o == L['end']
    return listing, img


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('out', nargs='?')
    ap.add_argument('--keys', required=True)
    ap.add_argument('--gap', type=int, default=30)
    ap.add_argument('--mode', choices=('dos', 'bios'), default='dos')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    listing, img = code(parse_keys(a.keys, a.gap, a.mode), mode=a.mode)
    if a.list:
        for o, b, t in listing:
            print('%04X  %-20s %s' % (o, b.hex(' ').upper(), t))
    if a.out:
        open(a.out, 'wb').write(img)
    return 0


if __name__ == '__main__':
    sys.exit(main())
