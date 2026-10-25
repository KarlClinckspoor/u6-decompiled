"""Ultima VI LZW decompression (the format of LoadFile's "Z" files and decompress()).

4-byte little-endian uncompressed size, then an LSB-first stream of 9..12-bit codes:
0x100 resets the dictionary, 0x101 ends the data, codes from 0x102 are dictionary entries.
"""
import struct


def decompress(data):
    size = struct.unpack_from('<I', data, 0)[0]
    src = data[4:]
    bitpos = 0
    nbits = 9
    out = bytearray()
    prefix = {}
    suffix = {}

    def string(code):
        s = bytearray()
        while code > 0xff:
            s.append(suffix[code])
            code = prefix[code]
        s.append(code)
        s.reverse()
        return s

    def getcode():
        nonlocal bitpos
        byte = bitpos >> 3
        v = src[byte] | (src[byte + 1] << 8 if byte + 1 < len(src) else 0) | \
            (src[byte + 2] << 16 if byte + 2 < len(src) else 0)
        v = (v >> (bitpos & 7)) & ((1 << nbits) - 1)
        bitpos += nbits
        return v

    next_code = 0x102
    limit = 0x200
    prev = None
    while True:
        code = getcode()
        if code == 0x100:
            nbits = 9
            limit = 0x200
            next_code = 0x102
            code = getcode()
            out += bytes([code])
            prev = code
            continue
        if code == 0x101:
            break
        if code < next_code:
            s = string(code)
            out += s
            c = s[0]
        else:
            s = string(prev)
            c = s[0]
            out += s + bytes([c])
        prefix[next_code] = prev
        suffix[next_code] = c
        next_code += 1
        if next_code >= limit and nbits < 12:
            nbits += 1
            limit <<= 1
        prev = code
    return bytes(out[:size]), size


if __name__ == '__main__':
    import sys
    d, n = decompress(open(sys.argv[1], 'rb').read())
    print(len(d), n)
    if len(sys.argv) > 2:
        open(sys.argv[2], 'wb').write(d)
