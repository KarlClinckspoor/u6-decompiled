#!/usr/bin/env python3
"""Put the fixups of a MASM 3.0 object in the order Turbo Assembler wrote them.

    python3 tools/u/tasmorder.py FILE.ASM FILE.OBJ      (rewrites FILE.OBJ)

U.EXE's hand-written assembly was assembled with TASM. MASM 3.0 makes the same bytes, but
it writes the fixups of each data record in the order of the code, and TASM did not
always: in some modules the fixups of forward references come first, in others last
(the rule could not be found without TASM itself). TLINK lays the .EXE relocation table
out in the order it meets the fixups, so this order is all that tells the two apart.

A module whose order differs says so in a comment, with the offsets (in the segment of
the module) of the words its fixups relocate, in TASM's order:

    ;TASM fixup order: 0046 000D 002A

Those fixups are put in that order, in the places the listed fixups have in their data
record (the others stay where they are). They must be in one record.
"""
import re, struct, sys


def _index(b, i):
    return (i + 2) if b[i] & 0x80 else (i + 1)


def subrecords(rec):
    """[(offset in the data record of the word the loader relocates, bytes)]"""
    out, i = [], 0
    while i < len(rec):
        start = i
        if not rec[i] & 0x80:
            raise ValueError('THREAD subrecords are not handled')
        loc = ((rec[i] & 3) << 8) | rec[i + 1]
        if (rec[i] >> 2) & 0xF == 3:
            loc += 2  # a far pointer: its segment word
        i += 2
        fd = rec[i]
        i += 1
        if not fd & 0x80 and ((fd >> 4) & 7) < 3:
            i = _index(rec, i)
        if not fd & 0x08:
            i = _index(rec, i)
        if not fd & 0x04:
            i += 2
        out.append((loc, rec[start:i]))
    return out


def wanted_order(asm):
    order = []
    for m in re.finditer(r';\s*TASM fixup order:([0-9A-Fa-f\s]+)', open(asm, encoding='latin-1').read()):
        order += [int(x, 16) for x in m.group(1).split()]
    return order


def fix(asm, obj):
    order = wanted_order(asm)
    if not order:
        return
    rank = {o: n for n, o in enumerate(order)}
    d = bytearray(open(obj, 'rb').read())
    out = bytearray()
    p, base, done = 0, 0, set()
    while p < len(d):
        t = d[p]
        ln = struct.unpack_from('<H', d, p + 1)[0]
        rec = bytes(d[p + 3:p + 2 + ln])
        if t in (0xA0, 0xA2):
            base = struct.unpack_from('<H', rec, _index(rec, 0))[0]
        if t == 0x9C:
            subs = subrecords(rec)
            slots = [n for n, (loc, _) in enumerate(subs) if base + loc in rank]
            moved = sorted((subs[n] for n in slots), key=lambda s: rank[base + s[0]])
            for n, s in zip(slots, moved):
                subs[n] = s
            done.update(base + loc for loc, _ in moved)
            body = b''.join(s for _, s in subs)
            L = len(body) + 1
            chk = (-(t + (L & 0xFF) + (L >> 8) + sum(body))) & 0xFF
            out += bytes([t]) + struct.pack('<H', L) + body + bytes([chk])
        else:
            out += d[p:p + 3 + ln]
        p += 3 + ln
    missing = [o for o in order if o not in done]
    if missing:
        raise SystemExit('%s: no fixup at %s' % (asm, ' '.join('%04X' % o for o in missing)))
    open(obj, 'wb').write(out)


if __name__ == '__main__':
    fix(sys.argv[1], sys.argv[2])
