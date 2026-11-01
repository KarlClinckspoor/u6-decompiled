#!/usr/bin/env python3
"""Undo Microsoft EXEPACK, to compare GAME.EXE as built (before EXEPACK) with the original.

    python3 tools/exepack.py PACKED.EXE [BUILT.EXE]

The original GAME.EXE is EXEPACKed. Unpacking gives its load image, relocation table,
CS:IP and SS:SP, which are what the linker produced. With a second file (a GAME.EXE as
tools/dosbuild.py links it), the two are compared: the image byte for byte over the
original's length (the build's longer image only adds the uninitialised data, zeros, which
EXEPACK drops), the relocations as a set (EXEPACK keeps them grouped by segment), and the
entry point and stack.
"""
import struct, sys


def load(path):
    """an .EXE: {img, rel (set of image addresses), cs, ip, ss, sp}"""
    d = open(path, 'rb').read()
    last, pages, nrel, hdr = struct.unpack_from('<4H', d, 2)
    ss, sp, _, ip, cs, relo = struct.unpack_from('<6H', d, 0x0E)
    size = (pages - 1) * 512 + (last or 512)
    rel = set()
    for i in range(nrel):
        off, seg = struct.unpack_from('<HH', d, relo + 4 * i)
        rel.add(seg * 16 + off)
    return dict(img=bytes(d[hdr * 16:size]), rel=rel, cs=cs, ip=ip, ss=ss, sp=sp)


def unpack(e):
    """the program an EXEPACKed .EXE (as load() returns it) unpacks into"""
    img = e['img']
    base = e['cs'] * 16
    rip, rcs, _memstart, _size, rsp, rss, dlen = struct.unpack_from('<7H', img, base)
    skip_len = 1
    if img[base + 14:base + 16] == b'RB':
        pass
    elif img[base + 16:base + 18] == b'RB':
        skip_len = struct.unpack_from('<H', img, base + 14)[0]
    else:
        raise ValueError('not an EXEPACKed file')
    buf = bytearray(img[:base]) + bytearray(max(0, dlen * 16 - base))
    src, dst = base - (skip_len - 1) * 16, dlen * 16
    while buf[src - 1] == 0xFF:
        src -= 1
    while True:
        cmd = buf[src - 1]
        n = buf[src - 3] | buf[src - 2] << 8
        src -= 3
        if cmd & 0xFE == 0xB0:      # fill n bytes
            b = buf[src - 1]
            src -= 1
            buf[dst - n:dst] = bytes([b]) * n
            dst -= n
        elif cmd & 0xFE == 0xB2:    # copy n bytes
            buf[dst - n:dst] = buf[src - n:src]
            dst -= n
            src -= n
        else:
            raise ValueError('bad EXEPACK command %02x' % cmd)
        if cmd & 1:
            break
    p = img.find(b'Packed file is corrupt', base)
    if p < 0:
        raise ValueError('no EXEPACK relocation table')
    p += len(b'Packed file is corrupt')
    rel = set()
    for seg in range(16):
        cnt = struct.unpack_from('<H', img, p)[0]
        p += 2
        for _ in range(cnt):
            rel.add(seg * 0x10000 + struct.unpack_from('<H', img, p)[0])
            p += 2
    return dict(img=bytes(buf[:dlen * 16]), rel=rel, cs=rcs, ip=rip, ss=rss, sp=rsp)


def compare(orig, built):
    """list of differences (empty: identical)"""
    out = []
    oi, ni = orig['img'], built['img']
    if len(ni) < len(oi):
        out.append('image %d bytes, the original %d' % (len(ni), len(oi)))
    diffs = [i for i in range(min(len(oi), len(ni))) if oi[i] != ni[i]]
    if diffs:
        out.append('%d image bytes differ, first at %05X' % (len(diffs), diffs[0]))
    if any(ni[len(oi):]):
        out.append('the image goes on past the original with non-zero bytes')
    if orig['rel'] != built['rel']:
        out.append('relocations differ: %d only in the original, %d only in the build' % (
            len(orig['rel'] - built['rel']), len(built['rel'] - orig['rel'])))
    for k in ('cs', 'ip', 'ss', 'sp'):
        if orig[k] != built[k]:
            out.append('%s %04X, the original %04X' % (k.upper(), built[k], orig[k]))
    return out


def main():
    o = unpack(load(sys.argv[1]))
    print('%s unpacked: image %d bytes, %d relocations, CS:IP %04X:%04X, SS:SP %04X:%04X' % (
        sys.argv[1], len(o['img']), len(o['rel']), o['cs'], o['ip'], o['ss'], o['sp']))
    if len(sys.argv) > 2:
        diffs = compare(o, load(sys.argv[2]))
        for d in diffs:
            print(d)
        print('identical to the original' if not diffs else 'DIFFERENT')
        sys.exit(1 if diffs else 0)


if __name__ == '__main__':
    main()
