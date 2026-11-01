"""Minimal OMF (Intel/Microsoft object module format) reader for Turbo C 2.0 / MASM
objects and libraries: segments with their data, fixups (masked when comparing),
public and external names."""
import struct

LOC_SIZE = {0: 1, 1: 2, 2: 2, 3: 4, 4: 1, 5: 2, 9: 4, 11: 6, 13: 4}


class Fixup:
    __slots__ = ('offset', 'loc', 'size', 'mode', 'frame', 'target', 'tkind', 'disp')

    def __repr__(self):
        return 'Fixup(%04x %s %s:%s+%x)' % (self.offset, self.loc, self.tkind, self.target, self.disp)


class Segment:
    def __init__(self, name, cls, length, align, combine):
        self.name, self.cls, self.length, self.align, self.combine = name, cls, length, align, combine
        self.data = bytearray(length)
        self.mask = bytearray(length)       # 1 where bytes were written
        self.fixups = []                    # Fixup with offset in segment

    def __repr__(self):
        return 'Segment(%s %s len=%x)' % (self.name, self.cls, self.length)


class Module:
    def __init__(self):
        self.name = ''
        self.lnames = ['']
        self.segs = [None]
        self.groups = [None]
        self.exts = [None]
        self.pubs = {}       # name -> (segindex, offset)
        self.comdefs = []


def _index(b, i):
    v = b[i]
    if v & 0x80:
        return ((v & 0x7f) << 8) | b[i + 1], i + 2
    return v, i + 1


def _name(b, i):
    n = b[i]
    return b[i + 1:i + 1 + n].decode('latin-1'), i + 1 + n


def parse_module(data, pos=0):
    """Parse one module starting at pos; returns (Module, next_pos)."""
    m = Module()
    last = None  # (seg, base offset) of last LEDATA/LIDATA
    threads = {}
    while pos < len(data):
        typ = data[pos]
        ln = struct.unpack_from('<H', data, pos + 1)[0]
        rec = data[pos + 3:pos + 3 + ln - 1]
        pos += 3 + ln
        if typ == 0x80:
            m.name, _ = _name(rec, 0)
        elif typ == 0x96:
            i = 0
            while i < len(rec):
                n, i = _name(rec, i)
                m.lnames.append(n)
        elif typ in (0x98, 0x99):
            attr = rec[0]
            i = 1
            align, comb, big = attr >> 5, (attr >> 2) & 7, (attr >> 1) & 1
            if align == 0:
                i += 3
            if typ == 0x98:
                length = struct.unpack_from('<H', rec, i)[0]; i += 2
            else:
                length = struct.unpack_from('<I', rec, i)[0]; i += 4
            if big:
                length = 0x10000
            nameidx, i = _index(rec, i)
            clsidx, i = _index(rec, i)
            m.segs.append(Segment(m.lnames[nameidx], m.lnames[clsidx], length, align, comb))
        elif typ == 0x9A:
            idx, i = _index(rec, 0)
            members = []
            while i < len(rec):
                i += 1
                s, i = _index(rec, i)
                members.append(s)
            m.groups.append((m.lnames[idx], members))
        elif typ in (0x8C, 0xB4):
            i = 0
            while i < len(rec):
                n, i = _name(rec, i)
                _, i = _index(rec, i)
                m.exts.append(n)
        elif typ in (0x90, 0x91):
            g, i = _index(rec, 0)
            s, i = _index(rec, i)
            if s == 0:
                i += 2
            while i < len(rec):
                n, i = _name(rec, i)
                if typ == 0x90:
                    off = struct.unpack_from('<H', rec, i)[0]; i += 2
                else:
                    off = struct.unpack_from('<I', rec, i)[0]; i += 4
                _, i = _index(rec, i)
                m.pubs[n] = (s, off)
        elif typ == 0xB0:
            i = 0
            while i < len(rec):
                n, i = _name(rec, i)
                _, i = _index(rec, i)
                kind = rec[i]; i += 1

                def num(i):
                    v = rec[i]
                    if v < 0x80:
                        return v, i + 1
                    if v == 0x81:
                        return struct.unpack_from('<H', rec, i + 1)[0], i + 3
                    if v == 0x84:
                        return rec[i + 1] | rec[i + 2] << 8 | rec[i + 3] << 16, i + 4
                    if v == 0x88:
                        return struct.unpack_from('<I', rec, i + 1)[0], i + 5
                    raise ValueError(v)
                if kind == 0x61:
                    a, i = num(i); b, i = num(i); size = a * b
                else:
                    size, i = num(i)
                m.exts.append(n)
                m.comdefs.append((n, size))
        elif typ in (0xA0, 0xA1):
            s, i = _index(rec, 0)
            if typ == 0xA0:
                off = struct.unpack_from('<H', rec, i)[0]; i += 2
            else:
                off = struct.unpack_from('<I', rec, i)[0]; i += 4
            seg = m.segs[s]
            chunk = rec[i:]
            seg.data[off:off + len(chunk)] = chunk
            seg.mask[off:off + len(chunk)] = b'\x01' * len(chunk)
            last = (seg, off)
        elif typ in (0xA2, 0xA3):
            s, i = _index(rec, 0)
            off = struct.unpack_from('<H', rec, i)[0]; i += 2
            seg = m.segs[s]

            def expand(i):
                rep = struct.unpack_from('<H', rec, i)[0]; i += 2
                blk = struct.unpack_from('<H', rec, i)[0]; i += 2
                if blk == 0:
                    n = rec[i]; out = rec[i + 1:i + 1 + n]; i += 1 + n
                else:
                    out = b''
                    for _ in range(blk):
                        sub, i = expand(i)
                        out += sub
                return out * rep, i
            out = b''
            while i < len(rec):
                o, i = expand(i)
                out += o
            seg.data[off:off + len(out)] = out
            seg.mask[off:off + len(out)] = b'\x01' * len(out)
            last = (seg, off)
        elif typ in (0x9C, 0x9D):
            i = 0
            while i < len(rec):
                b = rec[i]
                if not b & 0x80:  # THREAD
                    d = (b >> 6) & 1; meth = (b >> 2) & 7; num = b & 3
                    i += 1
                    idx = None
                    if meth < 3 or d == 0:
                        if not (d == 1 and meth >= 3):
                            if meth in (0, 1, 2) or (d == 0 and meth < 3):
                                idx, i = _index(rec, i)
                    threads[(d, num)] = (meth, idx)
                    continue
                locat = (b << 8) | rec[i + 1]
                i += 2
                mode = (locat >> 14) & 1
                loc = (locat >> 10) & 0xF
                doff = locat & 0x3FF
                fd = rec[i]; i += 1
                fthread = fd & 0x80; frame = (fd >> 4) & 7
                tthread = fd & 0x08; p = (fd >> 2) & 1; tgt = fd & 3
                fdatum = None
                if fthread:
                    fmeth, fdatum = threads.get((1, frame), (None, None))
                else:
                    fmeth = frame
                    if frame < 3:
                        fdatum, i = _index(rec, i)
                if tthread:
                    tmeth, tdatum = threads.get((0, tgt), (None, None))
                    tmeth = (tmeth or 0) | (p << 2)
                else:
                    tmeth = tgt | (p << 2)
                    tdatum, i = _index(rec, i)
                disp = 0
                if not (tmeth & 4):
                    disp = struct.unpack_from('<H', rec, i)[0]; i += 2
                f = Fixup()
                seg, base = last
                f.offset = base + doff
                f.loc = loc
                f.size = LOC_SIZE.get(loc, 2)
                f.mode = mode
                f.frame = (fmeth, fdatum)
                kind = tmeth & 3
                f.tkind = ('seg', 'grp', 'ext', 'frame')[kind]
                if kind == 0:
                    f.target = m.segs[tdatum].name
                elif kind == 1:
                    f.target = m.groups[tdatum][0]
                elif kind == 2:
                    f.target = m.exts[tdatum]
                else:
                    f.target = tdatum
                f.disp = disp
                seg.fixups.append(f)
        elif typ in (0x8A, 0x8B):
            return m, pos
    return m, pos


def parse_obj(path):
    data = open(path, 'rb').read()
    m, _ = parse_module(data)
    return m


def parse_lib(path):
    data = open(path, 'rb').read()
    assert data[0] == 0xF0
    page = struct.unpack_from('<H', data, 1)[0] + 3
    mods = []
    pos = page
    while pos < len(data) and data[pos] != 0xF1:
        if data[pos] != 0x80:
            pos += 1
            continue
        m, end = parse_module(data, pos)
        mods.append(m)
        pos = ((end + page - 1) // page) * page
    return mods
