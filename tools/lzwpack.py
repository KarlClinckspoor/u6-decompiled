#!/usr/bin/env python3
"""Ultima VI LZW compression: the inverse of port/tests/u6lzw.py, byte for byte.

The format (LoadFile's "Z" files, the drivers *.DRV and U.EXE's *DRV.BIN): a 4-byte
little-endian uncompressed size, then an LSB-first stream of 9..12-bit codes. 0x100 resets
the dictionary, 0x101 ends the data, codes from 0x102 are dictionary entries; the code width
grows when the next free code reaches 0x200, 0x400 and 0x800.

Origin's compressor was a plain greedy LZW: the longest known string, one new entry per code,
and a reset (0x100) as soon as the 4096-entry table is full. This reproduces all fifteen
compressed driver files of the GOG release (U6*.DRV, *DRV.BIN), so a rebuilt driver can be
compressed back to the shipped file.

    python3 tools/lzwpack.py IN OUT              compress IN into OUT
    python3 tools/lzwpack.py --check FILE...     decompress each FILE, compress it again and
                                                 compare with FILE (exit 1 on a difference)
"""
import os, struct, sys

MAX_CODES = 4096


def codes(data):
    """The code sequence for DATA, as Origin's compressor makes it."""
    out = [0x100]
    table = {}
    next_code = 0x102
    w = None
    for ch in data:
        if w is None:
            w = ch
            continue
        if (w, ch) in table:
            w = table[(w, ch)]
            continue
        out.append(w)
        if next_code < MAX_CODES:
            table[(w, ch)] = next_code
            next_code += 1
        else:                                   # the table is full: reset
            out.append(0x100)
            table = {}
            next_code = 0x102
        w = ch
    if w is not None:
        out.append(w)
    out.append(0x101)
    return out


def pack(data):
    """DATA compressed: the size word, then the codes at their widths."""
    out = bytearray(struct.pack('<I', len(data)))
    acc = nb = 0
    nbits, next_code, limit, first = 9, 0x102, 0x200, True
    for c in codes(data):
        acc |= c << nb
        nb += nbits
        while nb >= 8:
            out.append(acc & 0xff)
            acc >>= 8
            nb -= 8
        if c == 0x100:
            nbits, next_code, limit, first = 9, 0x102, 0x200, True
            continue
        if c == 0x101:
            break
        if first:                               # the first code after a reset adds no entry
            first = False
            continue
        next_code += 1
        if next_code >= limit and nbits < 12:
            nbits += 1
            limit <<= 1
    if nb:
        out.append(acc & 0xff)
    return bytes(out)


def main(argv):
    if len(argv) >= 2 and argv[0] == '--check':
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'port', 'tests'))
        import u6lzw
        bad = 0
        for path in argv[1:]:
            raw = open(path, 'rb').read()
            data = u6lzw.decompress(raw)
            data = data[0] if isinstance(data, tuple) else data
            same = pack(bytes(data)) == raw
            bad += not same
            print('%-14s %6d -> %6d bytes  %s' % (os.path.basename(path), len(raw), len(data),
                                                 'identical' if same else 'DIFFERENT'))
        return 1 if bad else 0
    if len(argv) == 2:
        open(argv[1], 'wb').write(pack(open(argv[0], 'rb').read()))
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
