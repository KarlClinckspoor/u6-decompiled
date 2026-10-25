#!/usr/bin/env python3
"""Differential test: port/gfx/mcga.c against the original U6MCGA.DRV.

The original driver (unpacked from /home/claude/gamedata/U6MCGA.DRV) runs in unicorn in
an emulated real-mode megabyte; the C (built with port/tests/mcga_shim.c into a shared
library) runs with its own DOS arena. Both get the same memory contents and the same
callbacks (file loading from the game data, stand-ins for DISK_confirm, LoadFileGr,
OSI_open/read/close); every entry is called on both with the same arguments, and after
each call the screen (A000:0000-FFFF), the arena from 3000:0000 to A000:0000 (back
buffer, tile memory, cache pages, sprites, mouse save buffer...), the driver's own
segment, tGr.pDriverMem, the callback calls made and the returned values are compared.

  python3 port/tests/mcga_diff.py [--ops N] [--seed S] [--quick]

Exit status 0 when everything matched.
"""
import argparse
import ctypes
import os
import random
import struct
import subprocess
import sys
import tempfile

from unicorn import (Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_CODE, UC_HOOK_INTR, UC_HOOK_BLOCK,
                     UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE)
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX, UC_X86_REG_DX,
                               UC_X86_REG_SI, UC_X86_REG_DI, UC_X86_REG_BP, UC_X86_REG_SP,
                               UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
                               UC_X86_REG_EFLAGS, UC_X86_REG_IP)

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
GAMEDATA = os.environ.get('U6_GAMEDATA', '/home/claude/gamedata')
sys.path.insert(0, HERE)
import u6lzw  # noqa: E402

# ---- memory layout (linear addresses are seg * 16 + off) ----
DRVSEG = 0x2000          # the driver image
DGROUP = 0x1000          # game DS = SS
STUBSEG = 0xF000         # callback stubs and the return trap
RET_TRAP = 0x0100
WORK = (0x3000, 0x0004)  # ScratchBuf: the 176x176 back buffer (its 64 KB segment holds nothing else)
FONT_SEG = 0x4008        # 4000:0000 is left free: see Emu._overrun
PAPER_SEG = 0x4100       # 320x200 pixmap
PORTRAIT_SEG = 0x5100    # 56x64 pixmap
SHAPES_SEG = 0x5200      # U6MCGA.PTR
SAVEBUF_SEG = 0x5300     # 0x400 bytes
ODDPIX_SEG = 0x5340      # 57x5 pixmap (odd width)
SYNSHAPES_SEG = 0x5360   # synthetic pointer shapes with RLE runs (U6MCGA.PTR has none)
DRVMEM_SEG = 0x5400      # GR_INIT_MEMORY
DRVMEM_MAX = 0xA000 - DRVMEM_SEG
SCREENSCAN = 0x8000      # DGROUP offsets
MAPSCAN = 0x8200
SURF_SCREEN = 0x9000
SURF_MAP = 0x9010
CMP_LO, CMP_HI = 0x30000, 0xA0000
CMP_LOW = (0x00000, 0x01000)   # also compared: a negative sprite slot writes at 0000:0000

# callback stubs: offset -> (name, bytes popped)
STUBS = {0x00: ('disk', 2), 0x10: ('load', 10), 0x20: ('open', 2), 0x30: ('read', 14), 0x40: ('close', 2)}
LOAD_RESULT = 0x1234     # what the LoadFileGr stand-in returns (AX)

# the parts of the unpacked driver that are data, not code (offsets in its segment)
DATA_RANGES = [(0x0051, 0x2198), (0x21FC, 0x2249), (0x26B6, 0x2764), (0x28EE, 0x290E),
               (0x2A69, 0x2A72), (0x2C00, 0x2CC2), (0x2D67, 0x2D80), (0x2F14, 0x30A4),
               (0x35E4, 0x10000)]
# compared separately (tGr) or kept in locals by the C (the mouse code's temporaries)
DRV_EXCLUDE = [(0x0052, 0x0086), (0x2F20, 0x2F4A)]


def gamefile(name):
    want = name.lower()
    for f in os.listdir(GAMEDATA):
        if f.lower() == want:
            with open(os.path.join(GAMEDATA, f), 'rb') as fh:
                return fh.read()
    raise FileNotFoundError(name)


def unz(name):
    return u6lzw.decompress(gamefile(name))[0]


def build_lib(tmp):
    out = os.path.join(tmp, 'libmcga_test.so')
    srcs = [os.path.join(PORT, 'gfx', 'mcga.c'), os.path.join(PORT, 'gfx', 'vgadac.c'),
            os.path.join(HERE, 'mcga_shim.c')]
    cc = os.environ.get('CC', 'cc')
    subprocess.check_call([cc, '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2', '-shared', '-fPIC',
                           '-o', out] + srcs)
    return out


class Files:
    """The game files the callbacks hand out (shared by both sides)."""
    def __init__(self):
        self.handles = {}
        self.next = 5

    def open(self, name):
        try:
            data = gamefile(name)
        except FileNotFoundError:
            return -1
        h = self.next
        self.next += 1
        self.handles[h] = data
        return h

    def read(self, h, offset, length):
        return self.handles[h][offset:offset + length]

    def close(self, h):
        self.handles.pop(h, None)
        return 0

    @staticmethod
    def load(name, z):
        return unz(name) if z else gamefile(name)


# ---- the original, in unicorn ----

class Emu:
    def __init__(self, image):
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_16)
        mu.mem_map(0, 0x110000)
        mu.mem_write(DRVSEG * 16, image)
        for off, (_, pop) in STUBS.items():
            mu.mem_write(STUBSEG * 16 + off, bytes([0xCA, pop & 0xFF, pop >> 8]))
        mu.mem_write(STUBSEG * 16 + RET_TRAP, b'\xF4')
        mu.hook_add(UC_HOOK_CODE, self._stub, begin=STUBSEG * 16, end=STUBSEG * 16 + 0x4F)
        mu.hook_add(UC_HOOK_INTR, self._intr)
        self.covered = bytearray(0x10000)   # driver bytes executed
        mu.hook_add(UC_HOOK_BLOCK, self._block, begin=DRVSEG * 16, end=DRVSEG * 16 + 0xFFFF)
        # a word access at offset FFFFh of the screen or the back buffer segment touches
        # the byte after the segment: a 286+ faults there, unicorn does not wrap; such
        # inputs are not valid tests
        for seg in (0xA000, WORK[0]):
            mu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self._overrun,
                        begin=seg * 16 + 0x10000, end=seg * 16 + 0x10000)
        self.files = Files()
        self.log = []
        self.error = None

    def _overrun(self, mu, access, address, size, value, user):
        self.error = 'invalid test input: word access at offset FFFFh (%05X)' % address
        mu.emu_stop()

    def _block(self, mu, address, size, user):
        o = address - DRVSEG * 16
        self.covered[o:o + size] = b'\1' * size

    def r16(self, lin):
        return struct.unpack('<H', self.mu.mem_read(lin, 2))[0]

    def w16(self, lin, v):
        self.mu.mem_write(lin, struct.pack('<H', v & 0xFFFF))

    def read(self, lin, n):
        return bytes(self.mu.mem_read(lin, n))

    def write(self, lin, data):
        self.mu.mem_write(lin, bytes(data))

    def _cstr(self, lin):
        s = self.read(lin, 13)
        return s.split(b'\0')[0].decode()

    def _where(self, seg, off):
        if seg == DRVSEG:
            return ('drv', off)
        return ('lin', seg * 16 + off)

    def _stub(self, mu, address, size, user):
        off = address - STUBSEG * 16
        if off not in STUBS:
            return
        sp = mu.reg_read(UC_X86_REG_SP)
        ss = mu.reg_read(UC_X86_REG_SS)
        arg = lambda i: self.r16(ss * 16 + ((sp + 4 + 2 * i) & 0xFFFF))  # noqa: E731
        name = STUBS[off][0]
        if name == 'disk':
            self.log.append(('disk', arg(0)))
            mu.reg_write(UC_X86_REG_AX, 0)
        elif name == 'load':
            isz, boff, bseg, noff, nseg = (arg(i) for i in range(5))
            fname = self._cstr(nseg * 16 + noff)
            data = Files.load(fname, isz)
            self.log.append(('load', fname, self._where(bseg, boff), isz))
            self.write(bseg * 16 + boff, data)
            mu.reg_write(UC_X86_REG_AX, LOAD_RESULT)
        elif name == 'open':
            ds = mu.reg_read(UC_X86_REG_DS)
            fname = self._cstr(ds * 16 + arg(0))
            h = self.files.open(fname)
            self.log.append(('open', fname))
            mu.reg_write(UC_X86_REG_AX, h & 0xFFFF)
        elif name == 'read':
            boff, bseg, llo, lhi, olo, ohi, h = (arg(i) for i in range(7))
            data = self.files.read(h, olo | ohi << 16, llo | lhi << 16)
            self.log.append(('read', olo | ohi << 16, llo | lhi << 16, self._where(bseg, boff)))
            self.write(bseg * 16 + boff, data)
            mu.reg_write(UC_X86_REG_AX, len(data) & 0xFFFF)
            mu.reg_write(UC_X86_REG_DX, len(data) >> 16)
        elif name == 'close':
            self.log.append(('close',))
            self.files.close(arg(0))
            mu.reg_write(UC_X86_REG_AX, 0)

    def _intr(self, mu, intno, user):
        ax = mu.reg_read(UC_X86_REG_AX)
        if intno == 0x10 and ax >> 8 == 0x0F:
            mu.reg_write(UC_X86_REG_AX, 0x5003)
            mu.reg_write(UC_X86_REG_BX, mu.reg_read(UC_X86_REG_BX) & 0x00FF)
        elif intno == 0x10 and ax >> 8 == 0x00:
            if ax & 0x7F == 0x13:
                self.write(0xA0000, bytes(0x10000))
        else:
            self.error = 'unexpected int %02Xh AX=%04X' % (intno, ax)
            mu.emu_stop()

    def call(self, entry, words, limit=0):
        """words: the 16-bit arguments in push order. Returns AX, DX, SP balance."""
        mu = self.mu
        sp = 0xFFF0
        for w in words:
            sp -= 2
            self.w16(DGROUP * 16 + sp, w)
        sp -= 4
        self.w16(DGROUP * 16 + sp, RET_TRAP)
        self.w16(DGROUP * 16 + sp + 2, STUBSEG)
        for reg, v in ((UC_X86_REG_CS, DRVSEG), (UC_X86_REG_DS, DGROUP), (UC_X86_REG_SS, DGROUP),
                       (UC_X86_REG_ES, DGROUP), (UC_X86_REG_SP, sp), (UC_X86_REG_EFLAGS, 0x0002),
                       (UC_X86_REG_AX, 0), (UC_X86_REG_BX, 0), (UC_X86_REG_CX, 0), (UC_X86_REG_DX, 0),
                       (UC_X86_REG_SI, 0), (UC_X86_REG_DI, 0), (UC_X86_REG_BP, 0)):
            mu.reg_write(reg, v)
        self.log = []
        self.error = None
        mu.emu_start(DRVSEG * 16 + entry, STUBSEG * 16 + RET_TRAP, count=limit)
        if self.error:
            raise RuntimeError(self.error)
        if mu.reg_read(UC_X86_REG_CS) != STUBSEG or mu.reg_read(UC_X86_REG_IP) != RET_TRAP:
            raise RuntimeError('entry %02Xh did not return' % entry)
        return (mu.reg_read(UC_X86_REG_AX), mu.reg_read(UC_X86_REG_DX),
                mu.reg_read(UC_X86_REG_SP) - 0xFFF0)


# ---- the C ----

class Info(ctypes.Structure):
    _pack_ = 1  # mcga.h packs it, as the game's tGr
    _fields_ = [('header', ctypes.c_uint8 * 6), ('font', ctypes.c_void_p),
                ('textFg', ctypes.c_uint8), ('textBg', ctypes.c_uint8), ('xorMode', ctypes.c_uint8),
                ('unknown0d', ctypes.c_uint8), ('animOn', ctypes.c_uint8), ('toBackBuffer', ctypes.c_uint8),
                ('pfnDiskConfirm', ctypes.c_void_p), ('reserved14', ctypes.c_uint8 * 8),
                ('pWorkBuf', ctypes.c_void_p), ('pDriverMem', ctypes.c_void_p),
                ('pfnLoadFile', ctypes.c_void_p), ('pfnOpen', ctypes.c_void_p),
                ('pfnRead', ctypes.c_void_p), ('pfnClose', ctypes.c_void_p)]


class Surface(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('segment', ctypes.c_uint16), ('pScanlines', ctypes.c_void_p),
                ('right', ctypes.c_uint16), ('left', ctypes.c_uint16),
                ('top', ctypes.c_uint16), ('bottom', ctypes.c_uint16)]


DISKFN = ctypes.CFUNCTYPE(ctypes.c_int16, ctypes.c_int16)
LOADFN = ctypes.CFUNCTYPE(ctypes.c_int16, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int16)
OPENFN = ctypes.CFUNCTYPE(ctypes.c_int16, ctypes.c_void_p)
READFN = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_int16, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p)
CLOSEFN = ctypes.CFUNCTYPE(ctypes.c_int16, ctypes.c_int16)

I16, U16, VP = ctypes.c_int16, ctypes.c_uint16, ctypes.c_void_p
PROTOS = {
    0x00: (VP, [I16]), 0x03: (None, []), 0x06: (None, [I16]), 0x09: (None, [I16, I16]),
    0x0C: (None, [VP, VP, I16, I16, VP, U16]), 0x0F: (None, [VP, VP, I16, I16, VP, U16]),
    0x12: (None, [I16] * 4), 0x15: (None, [I16] * 5), 0x18: (None, [VP, I16, I16]),
    0x1B: (None, [I16, I16]), 0x1E: (None, [I16] * 3), 0x21: (None, []), 0x24: (None, []),
    0x27: (None, [I16]), 0x2A: (I16, [U16, U16]), 0x2D: (None, [I16] * 3),
    0x30: (None, [VP, I16, I16, VP, U16]), 0x33: (None, [I16] * 5), 0x36: (None, [I16] * 3),
    0x39: (None, [I16] * 3), 0x3C: (None, []), 0x3F: (None, []), 0x42: (None, [I16] * 3),
    0x45: (None, [I16] * 6), 0x48: (None, [I16] * 4), 0x4B: (None, [I16, I16]), 0x4E: (None, []),
}


class CDrv:
    def __init__(self, libpath):
        self.lib = lib = ctypes.CDLL(libpath)
        lib.shim_arena.restype = ctypes.c_void_p
        lib.mcga_screen.restype = ctypes.c_void_p
        lib.mcga_segment.restype = ctypes.c_void_p
        lib.shim_video_pixels.restype = ctypes.c_void_p
        lib.shim_video_palette.restype = ctypes.c_void_p
        lib.mcga_attach_image.argtypes = [VP, ctypes.c_uint32]
        lib.dosmem_init()
        assert lib.shim_info_size() == ctypes.sizeof(Info), 'struct McgaInfo layout'
        assert lib.shim_surface_size() == ctypes.sizeof(Surface), 'struct McgaSurface layout'
        self.arena = lib.shim_arena()
        self.screen = lib.mcga_screen()
        self.seg = lib.mcga_segment()
        self.fns = {}
        for e, (res, args) in PROTOS.items():
            f = getattr(lib, 'GRDRV_%02X' % e)
            f.restype = res
            f.argtypes = args
            self.fns[e] = f
        self.files = Files()
        self.log = []
        self.keep = [DISKFN(self._disk), LOADFN(self._load), OPENFN(self._open),
                     READFN(self._read), CLOSEFN(self._close)]
        self.info = None

    def ptr(self, seg, off=0):
        return self.arena + seg * 16 + off

    def read(self, lin, n):
        return ctypes.string_at(self.arena + lin, n)

    def write(self, lin, data):
        data = bytes(data)
        ctypes.memmove(self.arena + lin, data, len(data))

    def _where(self, p):
        if self.arena <= p < self.arena + 0x110000:
            return ('lin', p - self.arena)
        if self.seg <= p < self.seg + 0x10000:
            return ('drv', p - self.seg)
        return ('host', p)

    def _disk(self, n):
        self.log.append(('disk', n & 0xFFFF))
        return 0

    def _load(self, name, dest, z):
        fname = ctypes.string_at(name, 13).split(b'\0')[0].decode()
        data = Files.load(fname, z)
        self.log.append(('load', fname, self._where(dest), z & 0xFFFF))
        ctypes.memmove(dest, data, len(data))
        return LOAD_RESULT

    def _open(self, path):
        fname = ctypes.string_at(path).decode()
        self.log.append(('open', fname))
        return self.files.open(fname)

    def _read(self, h, offset, length, buf):
        data = self.files.read(h, offset, length)
        self.log.append(('read', offset, length, self._where(buf)))
        ctypes.memmove(buf, data, len(data))
        return len(data)

    def _close(self, h):
        self.log.append(('close',))
        return self.files.close(h)

    def install(self, info_ptr):
        self.info = Info.from_address(info_ptr)
        i = self.info
        i.pfnDiskConfirm = ctypes.cast(self.keep[0], VP).value
        i.pfnLoadFile = ctypes.cast(self.keep[1], VP).value
        i.pfnOpen = ctypes.cast(self.keep[2], VP).value
        i.pfnRead = ctypes.cast(self.keep[3], VP).value
        i.pfnClose = ctypes.cast(self.keep[4], VP).value


# ---- a session: both drivers side by side ----

def ranges_minus(ranges, exclude):
    out = []
    for lo, hi in ranges:
        cur = [(lo, hi)]
        for elo, ehi in exclude:
            nxt = []
            for a, b in cur:
                if ehi <= a or elo >= b:
                    nxt.append((a, b))
                else:
                    if a < elo:
                        nxt.append((a, elo))
                    if ehi < b:
                        nxt.append((ehi, b))
            cur = nxt
        out += cur
    return out


class Mismatch(Exception):
    pass


class Session:
    def __init__(self, libpath, image, attach):
        self.image = image
        self.attach = attach
        self.emu = Emu(image)
        self.c = CDrv(libpath)
        self._img_buf = ctypes.create_string_buffer(image, len(image))
        self.c.lib.mcga_attach_image(self._img_buf if attach else None, len(image) if attach else 0)
        self.drv_ranges = ranges_minus([(0, 0x10000)] if attach else DATA_RANGES, DRV_EXCLUDE)
        self.count = {}
        self.reads = 0
        self.calls = 0
        self.history = []
        self.surfaces = {}

    # -- memory helpers on both sides --
    def put(self, lin, data):
        self.emu.write(lin, data)
        self.c.write(lin, data)

    def far(self, seg, off=0):
        """(words pushed for a far pointer, the C pointer)"""
        return [seg, off], self.c.ptr(seg, off)

    def compare(self, what):
        e, c = self.emu, self.c
        sv_e, sv_c = e.read(0xA0000, 0x10000), ctypes.string_at(c.screen, 0x10000)
        if sv_e != sv_c:
            self.fail(what, 'screen', sv_e, sv_c, 0xA0000)
        ae, ac = e.read(CMP_LO, CMP_HI - CMP_LO), c.read(CMP_LO, CMP_HI - CMP_LO)
        if ae != ac:
            self.fail(what, 'arena', ae, ac, CMP_LO)
        le, lc = e.read(CMP_LOW[0], CMP_LOW[1] - CMP_LOW[0]), c.read(CMP_LOW[0], CMP_LOW[1] - CMP_LOW[0])
        if le != lc:
            self.fail(what, 'low memory', le, lc, CMP_LOW[0])
        de = e.read(DRVSEG * 16, 0x10000)
        dc = ctypes.string_at(c.seg, 0x10000)
        for lo, hi in self.drv_ranges:
            if de[lo:hi] != dc[lo:hi]:
                self.fail(what, 'driver segment', de[lo:hi], dc[lo:hi], lo)
        pe = e.r16(DRVSEG * 16 + 0x74) * 16 + e.r16(DRVSEG * 16 + 0x72)
        pc = c.info.pDriverMem
        pc = 0 if not pc else pc - c.arena
        if pe != pc:
            raise Mismatch('%s: pDriverMem %05X vs %05X' % (what, pe, pc))
        return sv_e

    def fail(self, what, region, a, b, base):
        diffs = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
        i = diffs[0]
        raise Mismatch('%s: %s differs at %05X (%d bytes): DOS %s / C %s' % (
            what, region, base + i, len(diffs), a[i:i + 16].hex(), b[i:i + 16].hex()))

    def call(self, entry, ewords, cargs, check_ret=None, limit=0):
        what = 'GRDRV_%02X%r' % (entry, tuple(x if not isinstance(x, int) or x < 0x10000 else hex(x) for x in ewords))
        self.history.append(what)
        before = self.emu.read(0xA0000, 0x10000)
        d0 = self.c.lib.shim_dirty_count()
        ax, dx, spd = self.emu.call(entry, ewords, limit)
        self.c.log = []
        r = self.c.fns[entry](*cargs)
        if entry == 0x00:
            self.c.install(r)
        if self.emu.log != self.c.log:
            raise Mismatch('%s: callbacks DOS %r / C %r' % (what, self.emu.log, self.c.log))
        after = self.compare(what)
        if after != before and self.c.lib.shim_dirty_count() == d0:
            raise Mismatch('%s: screen changed without plat_video_dirty()' % what)
        if check_ret is not None:
            check_ret(ax, dx, spd, r)
        self.reads += sum(1 for x in self.emu.log if x[0] == 'read')
        self.count[entry] = self.count.get(entry, 0) + 1
        self.calls += 1
        return ax, dx, spd, r

    # -- entries with typed arguments --
    def e(self, entry, *args):
        """args: ints (16-bit, pushed as one word) or ('far', seg, off) or ('surf', name)."""
        ew, ca = [], []
        for a in args:
            if isinstance(a, tuple) and a[0] == 'far':
                ew += [a[1], a[2]]
                ca.append(self.c.ptr(a[1], a[2]))
            elif isinstance(a, tuple) and a[0] == 'surf':
                at, s = self.surfaces[a[1]]
                ew.append(at)
                ca.append(ctypes.cast(ctypes.pointer(s), VP).value)
            else:
                ew.append(a & 0xFFFF)
                ca.append(a)
        return self.call(entry, ew, ca)


def session_init(sess):
    """GR_INIT, keeping the C's struct pointer."""
    def chk(ax, dx, spd, r):
        if (dx, ax) != (DRVSEG, 0x52):
            raise Mismatch('GR_INIT returned %04X:%04X' % (dx, ax))
        sess._last_info = r
    sess.call(0x00, [0x13], [0x13], chk)
    # rest of init without calling GR_INIT again
    e = sess.emu
    sess.c.install(sess._last_info)
    for off, stub in ((0x62, 0x00), (0x76, 0x10), (0x7A, 0x20), (0x7E, 0x30), (0x82, 0x40)):
        e.w16(DRVSEG * 16 + off, stub)
        e.w16(DRVSEG * 16 + off + 2, STUBSEG)
    e.w16(DRVSEG * 16 + 0x58, 0)
    e.w16(DRVSEG * 16 + 0x5A, FONT_SEG)
    e.w16(DRVSEG * 16 + 0x6E, WORK[1])
    e.w16(DRVSEG * 16 + 0x70, WORK[0])
    sess.c.info.font = sess.c.ptr(FONT_SEG)
    sess.c.info.pWorkBuf = sess.c.ptr(*WORK)
    sess.put(FONT_SEG * 16, gamefile('U6.CH'))
    sess.put(PAPER_SEG * 16, unz('PAPER.BMP'))
    sess.put(SHAPES_SEG * 16, unz('U6MCGA.PTR'))
    rnd = random.Random(1)
    sess.put(WORK[0] * 16, bytes(rnd.randrange(256) for _ in range(0x10000)))
    sess.put(PORTRAIT_SEG * 16, struct.pack('<HH', 56, 64) + bytes(rnd.randrange(256) for _ in range(56 * 64)))
    sess.put(SAVEBUF_SEG * 16, bytes(0x400))
    sess.put(ODDPIX_SEG * 16, struct.pack('<HH', 57, 5) + bytes(rnd.randrange(256) for _ in range(57 * 5)))
    blob, sess.nsyn = synthetic_shapes(rnd)
    sess.put(SYNSHAPES_SEG * 16, blob)
    scr = struct.pack('<200H', *[i * 320 for i in range(200)])
    mp = struct.pack('<160H', *[(WORK[1] + 0x588 + i * 176) & 0xFFFF for i in range(160)])
    sess.put(DGROUP * 16 + SCREENSCAN, scr)
    sess.put(DGROUP * 16 + MAPSCAN, mp)
    sess.surface_objs = []
    for name, at, seg, tab, r, b in (('screen', SURF_SCREEN, 0xA000, SCREENSCAN, 319, 199),
                                    ('map', SURF_MAP, WORK[0], MAPSCAN, 159, 159)):
        e.write(DGROUP * 16 + at, struct.pack('<HHHHHHH', seg, tab, DGROUP, r, 0, 0, b))
        s = Surface(seg, sess.c.ptr(DGROUP, tab), r, 0, 0, b)
        sess.surface_objs.append(s)
        sess.surfaces[name] = (at, s)


def synthetic_shapes(r):
    """a pointer shape table like U6MCGA.PTR whose runs mix literal and RLE rows"""
    shapes = []
    for n in range(6):
        er, el, eu, ed = r.randrange(2, 14), r.randrange(2, 14), r.randrange(2, 12), r.randrange(2, 12)
        body = struct.pack('<4H', er, el, eu, ed)
        for y in range(-eu, ed + 1):
            if r.random() < 0.15:
                continue
            x = r.randrange(-el, er + 1)
            length = r.randrange(1, er - x + 2)
            if r.random() < 0.4 and n != 5:
                body += struct.pack('<3H', length * 2, x & 0xFFFF, y & 0xFFFF)
                body += bytes(r.randrange(256) for _ in range(length))
            else:
                data, left = b'', length
                while left:
                    k = r.randrange(1, min(left, 127) + 1)
                    if r.random() < 0.5:
                        data += bytes([k << 1 | 1, r.randrange(256)])
                    else:
                        data += bytes([k << 1]) + bytes(r.randrange(256) for _ in range(k))
                    left -= k
                body += struct.pack('<3H', length * 2 + 1, x & 0xFFFF, y & 0xFFFF) + data
        body += struct.pack('<H', 0)
        shapes.append(body)
    table_len = 4 + 4 * len(shapes)
    out, offs = b'', []
    for b in shapes:
        offs.append(table_len + len(out))
        out += b
    blob = struct.pack('<I', table_len + len(out)) + b''.join(struct.pack('<I', o) for o in offs) + out
    assert len(blob) <= (DRVMEM_SEG - SYNSHAPES_SEG) * 16
    return blob, len(shapes)


def set_field(sess, name, value):
    """a tGr byte field the game writes (both sides)"""
    off = {'textFg': 0x5C, 'textBg': 0x5D, 'xorMode': 0x5E, 'toBackBuffer': 0x61}[name]
    sess.emu.write(DRVSEG * 16 + off, bytes([value]))
    setattr(sess.c.info, name, value)


# ---- the tests ----

ANIM = None


def animdata():
    global ANIM
    if ANIM is None:
        a = gamefile('ANIMDATA')
        n = struct.unpack_from('<H', a)[0]
        tiles = struct.unpack_from('<32H', a, 2)[:n]
        first = struct.unpack_from('<32H', a, 66)[:n]
        mask = a[130:130 + n]
        ANIM = list(zip(tiles, first, mask))
    return ANIM


def rand_tile(r):
    t = r.randrange(0x800)
    k = r.random()
    if k < 0.15:
        t |= r.choice([0x800, 0x1000, 0x1800])
    elif k < 0.2:
        t |= r.choice([0x2000, 0x4000, 0x6000, 0x2800, 0x7800])
    return t


def run_random(sess, r, nops, wild):
    """nops random calls of every entry; wild: also out-of-range arguments"""
    S = sess
    mouse_pos = {}
    for n in range(nops):
        k = r.randrange(100)
        if k < 14:
            x = r.randrange(-16, 177) if wild or r.random() < 0.3 else r.randrange(0, 161)
            y = r.randrange(-8, 177) if wild else r.randrange(0, 161)
            if y < 0:
                x &= ~1   # an odd x could put a word access at offset FFFFh
            S.e(0x42, rand_tile(r), x, y)
        elif k < 22:
            t = r.randrange(0x800) if not (wild and r.random() < 0.1) else r.randrange(0x800, 0x1000)
            S.e(0x2D, t, r.randrange(0, 305), r.randrange(0, 185))
        elif k < 27:
            slot = r.randrange(16)
            S.e(0x33, slot, rand_tile(r) & 0x7FF if r.random() < 0.8 or not wild else rand_tile(r), r.randrange(256),
                r.choice([0x7F, 0x80, 0, 4, 8, 12, 15, r.randrange(256)]),
                r.choice([0x7F, 0x80, 0, 4, 8, 12, 15, r.randrange(256)]))
        elif k < 31:
            S.e(0x36, r.randrange(16) if r.random() < 0.95 else r.randrange(16, 40),
                r.randrange(-8, 185), r.randrange(-8, 185))
        elif k < 35:
            S.e(0x39, r.randrange(16) if r.random() < 0.95 else r.randrange(16, 40),
                r.randrange(-10, 190), r.randrange(-10, 190))
        elif k < 39:
            tile, first, mask = r.choice(animdata())
            if r.random() < 0.8:
                S.e(0x4B, tile, first + (r.randrange(256) & mask))
            else:
                S.e(0x4B, r.randrange(0x800), r.randrange(0x280))
        elif k < 42:
            S.e(0x3F)
        elif k < 49:
            set_field(S, 'toBackBuffer', r.random() < 0.5)
            set_field(S, 'xorMode', r.random() < 0.3)
            S.e(0x06, r.randrange(256) | (r.randrange(256) << 8 if r.random() < 0.2 else 0))
            if S.c.info.toBackBuffer:
                S.e(0x09, r.randrange(0, 176), r.randrange(0, 176))
            else:
                S.e(0x09, r.randrange(0, 320), r.randrange(0, 200))
        elif k < 56:
            set_field(S, 'toBackBuffer', r.random() < 0.5)
            set_field(S, 'xorMode', r.random() < 0.3)
            S.e(0x06, r.randrange(256))
            if S.c.info.toBackBuffer:
                x0, x1, y0, y1 = r.randrange(176), r.randrange(176), r.randrange(176), r.randrange(176)
            else:
                x0, x1, y0, y1 = r.randrange(320), r.randrange(320), r.randrange(200), r.randrange(200)
            if r.random() < 0.3:
                x1 = x0 + r.choice([0, 1, 2])
            S.e(0x12, x0, y0, x1, y1)
        elif k < 60:
            x0, y0 = r.randrange(0, 300), r.randrange(0, 190)
            w, h = r.randrange(1, 320 - x0), r.randrange(2, 200 - y0)
            # h <= -dy: 65535/65536 lines, a word access at offset FFFFh at some point (#GP on a
            # 286+, DOS hangs; unicorn does not wrap): only aligned cases, in run_edges
            dy = r.choice([d for d in (-8, -8, -1, -3, -(h - 1), 0, 5) if h > -d])
            S.e(0x15, x0, y0, x0 + w - 1, y0 + h - 1, dy) if r.random() < 0.5 else \
                S.e(0x15, x0 + w - 1, y0 + h - 1, x0, y0, dy)
        elif k < 63:
            if r.random() < 0.3:
                S.e(0x18, ('far', PAPER_SEG, 0), 0, 0)
            else:
                # rows must not cross offset FFFFh with a word access (see above)
                y = r.randrange(0, 141)
                S.e(0x18, ('far', PORTRAIT_SEG, 0), r.randrange(0, 320), y)
        elif k < 70:
            set_field(S, 'textFg', r.randrange(256))
            set_field(S, 'textBg', r.randrange(256))
            S.e(0x1E, r.randrange(256), r.randrange(0, 313), r.randrange(0, 193))
        elif k < 77:
            x0, y0 = r.randrange(0, 176), r.randrange(0, 176)
            x1, y1 = r.randrange(x0, 176), r.randrange(y0, 176)
            S.e(0x45, x0, y0, x1, y1, r.randrange(0, max(1, 318 - (x1 - x0))), r.randrange(0, 200 - (y1 - y0)))
        elif k < 79:
            if r.random() < 0.5:
                S.e(0x48, 8, 8, 167, 167)
            else:
                x0, y0 = r.randrange(0, 170), r.randrange(0, 170)
                while True:
                    x1, y1 = r.randrange(x0, 176), r.randrange(y0, 176)
                    if (x1 - x0 + 1) * (y1 - y0 + 1) >= 3:
                        break
                S.e(0x48, x0, y0, x1, y1)
        elif k < 93:
            # the mouse: save, draw, restore at the same place (as MOUSE.ASM does)
            surf = r.choice(['screen', 'map'])
            w, h = (320, 200) if surf == 'screen' else (160, 160)
            x = r.randrange(-12, w + 12)
            y = r.randrange(-12, h + 12)
            shapes_seg = SHAPES_SEG if r.random() < 0.6 else SYNSHAPES_SEG
            shape = r.randrange(10 if shapes_seg == SHAPES_SEG else S.nsyn)
            key = surf
            if key in mouse_pos and r.random() < 0.7:
                px, py, pss, ps = mouse_pos.pop(key)
                S.e(0x0F, ('surf', surf), ('far', SAVEBUF_SEG, 0), px, py, ('far', pss, 0), ps)
            else:
                S.e(0x0C, ('surf', surf), ('far', SAVEBUF_SEG, 0), x, y, ('far', shapes_seg, 0), shape)
                S.e(0x30, ('surf', surf), x, y, ('far', shapes_seg, 0), shape)
                mouse_pos.clear()
                mouse_pos[key] = (x, y, shapes_seg, shape)
        elif k < 95:
            # pointer index out of the table: no drawing
            S.e(0x30, ('surf', r.choice(['screen', 'map'])), r.randrange(320), r.randrange(200),
                ('far', SHAPES_SEG, 0), r.randrange(11, 300))
        elif k < 96:
            S.e(0x1B, r.randrange(65536), r.randrange(65536))
            S.e(r.choice([0x21, 0x24, 0x3C, 0x03]))
        else:
            S.e(0x2D, r.randrange(0x200), r.randrange(0, 305), r.randrange(0, 185))


def run_game_like(sess, r, frames):
    """what the game does: the map window, the frame, missiles, the sky, the fade"""
    S = sess
    anim = animdata()
    for f in range(frames):
        for tile, first, mask in anim:
            S.e(0x4B, tile, first + ((f >> [3, 2, 1][f % 3]) & mask))
        S.e(0x3F)
        set_field(S, 'toBackBuffer', 0)
        set_field(S, 'xorMode', 0)
        S.e(0x06, 13)
        for j in range(11):
            for i in range(11):
                S.e(0x42, r.choice([r.randrange(0x200), r.randrange(0x200, 0x800)]), 8 + i * 16, 8 + j * 16)
                if r.random() < 0.3:
                    S.e(0x42, r.randrange(0x200, 0x800) | (r.choice([0, 0, 0, 4, 6]) << 10), 8 + i * 16, 8 + j * 16)
        for t, x, y in ((0x1B0, 0, 0), (0x1B2, 160, 0), (0x1B3, 0, 160), (0x1B5, 160, 160)):
            S.e(0x42, t, x, y)
        for i in range(1, 10):
            S.e(0x42, 0x1B1, i * 16, 0)
            S.e(0x42, 0x1B4, i * 16, 160)
            S.e(0x42, 0x1B6, 0, i * 16)
            S.e(0x42, 0x1B7, 160, i * 16)
        if f % 5 == 4:
            S.e(0x48, 8, 8, 167, 167)
        else:
            S.e(0x45, 8, 8, 167, 167, 8, 8)
        # a missile: rotated sprites, background saves
        S.e(0x33, 15, 0x23F, 0, 0x7F, 0x7F)
        for d in range(3):
            S.e(0x33, d + 8, 0x250 + d, r.randrange(128), 8, 8)
        pos = [(r.randrange(16, 160), r.randrange(16, 160)) for _ in range(3)]
        for step in range(4):
            for d in range(3):
                S.e(0x39, d, *pos[d])
            for d in range(3):
                S.e(0x36, d + 8, *pos[d])
            S.e(0x45, 8, 8, 167, 167, 8, 8)
            for d in range(3):
                S.e(0x36, d, *pos[d])
            pos = [(x + r.randrange(-6, 7), y + r.randrange(-6, 7)) for x, y in pos]
        # the sky bar and the status area
        for i in range(17):
            S.e(0x42, 0x170, i * 8, 4)
        S.e(0x42, 0x174, (19 - r.randrange(24)) << 3, r.randrange(4, 12))
        S.e(0x45, 0, 4, 135, 19, 176, 6)
        S.e(0x06, 0x31)
        S.e(0x12, 168, 6, 311, 103)
        for i in range(4):
            S.e(0x2D, 0x190 + i, 176 + i * 16, 88)
        # the console
        set_field(S, 'textFg', 0x48)
        set_field(S, 'textBg', 0x31)
        for i in range(20):
            S.e(0x1E, r.randrange(32, 128), 176 + 8 * (i % 16), 112 + 8 * (i // 16))
        S.e(0x15, 176, 112, 303, 183, -8)
        S.e(0x12, 176, 176, 303, 183)
        # an inventory slot with a number (DrawTileWithNumber)
        S.e(0x42, 0x1A0, 0, 0)
        S.e(0x42, r.randrange(0x200, 0x800), 0, 0)
        set_field(S, 'toBackBuffer', 1)
        S.e(0x06, 0x48)
        for py in range(11, 16):
            for px in range(r.randrange(13), 16):
                if r.random() < 0.5:
                    S.e(0x09, px, py)
        set_field(S, 'toBackBuffer', 0)
        S.e(0x45, 0, 0, 15, 15, 248, 32)
        # storm cloak noise and the spell flash (XOR into the back buffer)
        if f % 3 == 0:
            set_field(S, 'xorMode', 1)
            set_field(S, 'toBackBuffer', 1)
            S.e(0x06, 8)
            for i in range(50):
                S.e(0x09, r.randrange(8, 168), r.randrange(8, 168))
            S.e(0x06, 15)
            S.e(0x12, 8, 8, 168, 168)
            set_field(S, 'xorMode', 0)
            set_field(S, 'toBackBuffer', 0)
            S.e(0x45, 8, 8, 167, 167, 8, 8)


def run_edges(sess, wild):
    """specific cases: clipping edges, odd paths of the code"""
    S = sess
    for surf, (w, h) in (('screen', (320, 200)), ('map', (160, 160))):
        for shape in range(10):
            for x in (-20, -10, -5, -1, 0, 1, 5, w - 6, w - 1, w, w + 5, w + 12):
                for y in (-20, -8, -3, 0, 2, h - 9, h - 1, h, h + 4, h + 20):
                    S.e(0x0C, ('surf', surf), ('far', SAVEBUF_SEG, 0), x, y, ('far', SHAPES_SEG, 0), shape)
                    S.e(0x30, ('surf', surf), x, y, ('far', SHAPES_SEG, 0), shape)
                    if (x + y) % 3:
                        S.e(0x0F, ('surf', surf), ('far', SAVEBUF_SEG, 0), x, y, ('far', SHAPES_SEG, 0), shape)
    for surf, (w, h) in (('screen', (320, 200)), ('map', (160, 160))):
        for shape in range(S.nsyn):
            for x in (-20, -13, -7, -2, 0, 3, w - 9, w - 3, w - 1, w + 2, w + 13):
                for y in (-14, -5, 0, 1, h - 7, h - 1, h + 3, h + 14):
                    S.e(0x0C, ('surf', surf), ('far', SAVEBUF_SEG, 0), x, y, ('far', SYNSHAPES_SEG, 0), shape)
                    S.e(0x30, ('surf', surf), x, y, ('far', SYNSHAPES_SEG, 0), shape)
                    S.e(0x0F, ('surf', surf), ('far', SAVEBUF_SEG, 0), x, y, ('far', SYNSHAPES_SEG, 0), shape)
    for x, y in ((0, 0), (1, 10), (263, 100), (300, 150)):
        S.e(0x18, ('far', ODDPIX_SEG, 0), x, y)
    S.e(0x33, 16, 0x220, 5, 8, 8)
    S.e(0x33, 40, 0x220, 5, 8, 8)
    S.e(0x33, -1, 0x220, 5, 8, 8)
    S.e(0x33, -3, 0x221, 9, 7, 7)
    for slot in (0, 7, 15):
        for x in (0, 7, 8, 9, 100, 168, 175, 176, 183):
            for y in (0, 7, 8, 9, 100, 160, 167, 168, 175, 176):
                S.e(0x39, slot, x, y)
                S.e(0x36, slot, x, y)
    for t in list(range(0, 0x800, 7)) + [0x1FF, 0x200, 0x7FF]:
        S.e(0x2D, t, (t * 13) % 305, (t * 7) % 185)
        S.e(0x42, t, (t * 5) % 161, (t * 3) % 161)
    S.e(0x42, 0x234 | 0x800, 0, 0)
    S.e(0x42, 0x234 | 0x1000, 16, 0)
    S.e(0x42, 0x234 | 0x1800, 32, 0)
    for rot in range(0, 256, 5):
        S.e(0x33, rot % 16, 0x100 + rot, rot, rot % 16, (rot * 3) % 16)
        S.e(0x36, rot % 16, 40 + rot % 100, 50)
    S.e(0x4E)
    S.e(0x45, 8, 8, 167, 167, 8, 8)
    S.e(0x48, 0, 0, 1, 1)
    S.e(0x48, 0, 0, 2, 0)
    S.e(0x48, 10, 10, 10, 12)
    S.e(0x48, 0, 0, 175, 175)
    # fill: width 1 in XOR mode (the LOOP with CX = 0), corners swapped
    for tb in (0, 1):
        set_field(S, 'toBackBuffer', tb)
        set_field(S, 'xorMode', 1)
        S.e(0x06, 0x5A)
        S.e(0x12, 10, 10, 10, 20)
        S.e(0x12, 20, 30, 11, 25)
        S.e(0x12, 3, 3, 4, 4)
        set_field(S, 'xorMode', 0)
        S.e(0x12, 30, 40, 20, 35)
        S.e(0x09, 0, 0)
    set_field(S, 'toBackBuffer', 0)
    # scroll: height == -dy (65536 lines), dy > 0, dy == 0
    S.e(0x15, 100, 100, 107, 107, -8)
    S.e(0x15, 0, 0, 319, 199, 8)
    S.e(0x15, 0, 0, 319, 199, 0)
    S.e(0x15, 0, 0, 319, 199, -1)
    S.e(0x18, ('far', PORTRAIT_SEG, 0), 300, 190)
    S.e(0x1E, 0x41, 316, 199)
    S.e(0x2D, 0x100, 319, 199)
    S.e(0x45, 0, 0, 175, 175, 160, 100)
    # slots and frames out of range
    S.e(0x36, 16, 50, 50)
    S.e(0x39, -1, 50, 50)
    S.e(0x4B, 0x300, 0x200)
    S.e(0x4B, 0x300, 0x1FF)
    if wild:
        S.e(0x2D, 0x900, 10, 10)
        S.e(0x42, 0x10, -16, -16)
        S.e(0x42, 0x234, 170, 175)
        S.e(0x18, ('far', PORTRAIT_SEG, 0), 10, 230)
        S.e(0x1E, 0x141, 10, 10)
        S.e(0x09, 400, 250)
        S.e(0x2D, 0x10, 0, 210)
    S.call(0x1B, [1, 2], [1, 2])
    S.call(0x21, [], [])
    S.call(0x24, [], [])
    S.call(0x3C, [], [])
    S.call(0x03, [], [])

    def chk27(ax, dx, spd, r):
        if spd != -2:
            raise Mismatch('GRDRV_27 stack %d' % spd)
    S.call(0x27, [5], [5], chk27)


def run_memory(sess, size):
    def chk(ax, dx, spd, r):
        if ax != (r & 0xFFFF):
            raise Mismatch('GR_INIT_MEMORY returned %04X / %04X' % (ax, r & 0xFFFF))
    sess.call(0x2A, [DRVMEM_SEG, 0x2FFF], [DRVMEM_SEG, 0x2FFF], chk)
    sess.call(0x2A, [DRVMEM_SEG, size], [DRVMEM_SEG, size], chk)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ops', type=int, default=4000)
    ap.add_argument('--frames', type=int, default=6)
    ap.add_argument('--seed', type=int, default=6)
    ap.add_argument('--quick', action='store_true')
    a = ap.parse_args()
    if a.quick:
        a.ops, a.frames = 600, 2
    image, size = u6lzw.decompress(gamefile('U6MCGA.DRV'))
    assert len(image) == 13796
    total = {}
    covered = bytearray(0x10000)
    with tempfile.TemporaryDirectory() as tmp:
        lib = build_lib(tmp)
        configs = [
            ('image, %d KB driver memory' % (DRVMEM_MAX * 16 // 1024), True, DRVMEM_MAX, True),
            ('image, minimum driver memory (35 cache pages)', True, 0x3000, True),
            ('built-in tables', False, DRVMEM_MAX, False),
        ]
        ok = True
        for i, (name, attach, memsize, wild) in enumerate(configs):
            libcopy = os.path.join(tmp, 'lib%d.so' % i)   # a fresh copy: fresh static state
            with open(lib, 'rb') as src, open(libcopy, 'wb') as dst:
                dst.write(src.read())
            sess = Session(libcopy, image, attach)
            r = random.Random(a.seed + i)
            try:
                session_init(sess)
                if not attach:
                    # the built-in copy of the image's data
                    dc = ctypes.string_at(sess.c.seg, 0x10000)
                    img = image + bytes(0x10000 - len(image))
                    for lo, hi in DATA_RANGES:
                        if dc[lo:hi] != img[lo:hi] and (lo, hi) != (0x0051, 0x2198):
                            raise Mismatch('built-in data %04X-%04X' % (lo, hi))
                sess.e(0x18, ('far', PAPER_SEG, 0), 0, 0)
                run_memory(sess, memsize)
                run_game_like(sess, r, a.frames)
                run_edges(sess, wild)
                run_random(sess, r, a.ops, wild)
                run_game_like(sess, r, 1)
                print('%-50s OK   %6d calls' % (name, sess.calls))
            except Mismatch as m:
                ok = False
                print('%-50s FAIL after %d calls: %s' % (name, sess.calls, m))
                print('  last calls:', sess.history[-5:])
            for k, v in sess.count.items():
                total[k] = total.get(k, 0) + v
            for j in range(0x10000):
                covered[j] |= sess.emu.covered[j]
            print('  object tile block reads: %d' % sess.reads)
        # code bytes of the original never executed (the jump table counts as code)
        code = bytearray(len(image))
        for j in range(len(image)):
            code[j] = 1
        for lo, hi in DATA_RANGES:
            for j in range(lo, min(hi, len(image))):
                code[j] = 0
        gaps, j = [], 0
        while j < len(image):
            if code[j] and not covered[j]:
                k = j
                while k < len(image) and code[k] and not covered[k]:
                    k += 1
                gaps.append((j, k))
                j = k
            else:
                j += 1
        ncode = sum(code)
        ncov = sum(1 for j in range(len(image)) if code[j] and covered[j])
        print('original code executed: %d of %d bytes' % (ncov, ncode))
        if gaps:
            print('not executed:', ' '.join('%04X-%04X' % (lo, hi - 1) for lo, hi in gaps))
        print('calls per entry:', ' '.join('%02Xh:%d' % (k, total[k]) for k in sorted(total)))
        missing = sorted(set(PROTOS) - set(total))
        if missing:
            print('entries not called:', ' '.join('%02Xh' % k for k in missing))
            ok = False
    print('PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
