#!/usr/bin/env python3
"""Differential test: port/u/ugfx.c against U.EXE's original MCGADRV.BIN.

The original driver (MCGADRV.BIN from the game folder, LZW unpacked) runs in unicorn in
an emulated real-mode megabyte; the C (built with port/tests/ugfx_shim.c into a shared
library) runs with its own DOS arena. Both get the same memory contents (screen, off-screen
buffers, scanline tables, the shape files, the font U6.SET, the pointer save buffer) and
the same arguments; after every call the arena (all of it but U.EXE's DGROUP and the
driver's own segment, which hold the emulator's structs and stack), the driver's data
(CS:0000-029F: its variables, the run buffer at 0060, the color table at 01A0), the
struct tText (12h) and the scanline table (0) are compared.

The calls: entry 0; every shape of every .SHP file of the game and of INTRO.PTR drawn
(03h) unclipped and clipped on the screen, an off-screen buffer and random windows, and
its box saved and restored (06h, 09h); the indexes the bound check of 03h lets through
(the number of shapes, negative ones); random calls of every entry with edge cases
(windows partly or wholly outside the screen, negative and huge coordinates, empty
windows, overlapping copies, odd widths, every character of the font with the font's
colors or others); synthetic shapes for the odd cases (RLE runs longer than the run
buffer, a run word of 1, a box that lies, runs that never sum up).

Inputs that make the original touch a word at offset FFFFh of a segment (a 286+ faults
there, unicorn reads the byte after the segment) or run too long are not valid tests:
the original's memory is rolled back and the call is skipped (counted in the summary).

  python3 port/tests/ugfx_diff.py [--ops N] [--seed S] [--quick]

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

from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INTR, UC_HOOK_BLOCK, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
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
DGROUP = 0x1000          # U.EXE's DS = SS: the emulator's structs and stack
DRVSEG = 0x2000          # the driver image
SCAN_LIN = 0x30000       # scanline tables (word aligned: a table read never is at FFFFh)
BUFSEG = 0x4000          # off-screen pixels (SURF_alloc: offset 8 + y * width)
FONT_LIN = 0x50000       # U6.SET
FONT2_LIN = 0x52007      # a variant (font[2] == font[3]), at an odd offset
SAVE_LIN = 0x54000       # the pointer save buffer (64 KB)
SHAPE_LIN = 0x64000      # the shape file (up to 0x3C000 bytes)
SCREEN = 0xA000
STUBSEG = 0xF000
RET_TRAP = 0x0100

DG_SURF = (0x8000, 0x8010)   # the emulator's struct tSurface for the arguments
DG_TEXT = 0x8100             # its struct tText
DG_SCAN0 = 0x9000            # entry 0's table
CMP_RANGES = [(0x00000, 0x10000), (0x30000, 0x110000)]
DRV_CMP = [(0x001C, 0x0056), (0x0058, 0x005E), (0x0060, 0x02A0)]   # 0000-001B: the jump table; 56h: BX, 5Eh: DS
CODE = (0x02A0, 0x0A48)
LIMIT = 5000000

I16, VP = ctypes.c_int16, ctypes.c_void_p
PROTOS = {
    '0': [VP], '3': [VP, I16, I16, VP, I16], '6': [VP, VP, I16, I16, VP, I16],
    '9': [VP, VP, I16, I16, VP, I16], '0xc': [VP, I16], '0xf': [VP, VP],
    '0x12': [VP, I16, I16, I16, I16], '0x15': [VP], '0x18': [VP, I16, I16, I16],
}
ENTRY = {'0': 0x00, '3': 0x03, '6': 0x06, '9': 0x09, '0xc': 0x0C, '0xf': 0x0F,
         '0x12': 0x12, '0x15': 0x15, '0x18': 0x18}


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def gamefile(name):
    want = name.lower()
    for f in os.listdir(GAMEDATA):
        if f.lower() == want:
            with open(os.path.join(GAMEDATA, f), 'rb') as fh:
                return fh.read()
    raise FileNotFoundError(name)


def unz(name):
    return u6lzw.decompress(gamefile(name))[0]


def shape_files():
    names = sorted(f for f in os.listdir(GAMEDATA) if f.upper().endswith('.SHP'))
    return names + ['INTRO.PTR', 'U6MCGA.PTR']


def build_lib(tmp):
    out = os.path.join(tmp, 'libugfx_test.so')
    srcs = [os.path.join(PORT, 'u', 'ugfx.c'), os.path.join(HERE, 'ugfx_shim.c')]
    cc = os.environ.get('CC', 'cc')
    subprocess.check_call([cc, '-std=c99', '-Wall', '-Wextra', '-Werror', '-O2', '-shared', '-fPIC',
                           '-o', out] + srcs)
    return out


class Invalid(Exception):
    pass


# ---- the original, in unicorn ----

class Emu:
    WATCH = [SCREEN, BUFSEG, SAVE_LIN >> 4, FONT_LIN >> 4, FONT2_LIN >> 4]

    def __init__(self, image):
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_16)
        mu.mem_map(0, 0x110000)
        mu.mem_write(DRVSEG * 16, image)
        mu.hook_add(UC_HOOK_INTR, self._intr)
        self.covered = bytearray(0x10000)
        mu.hook_add(UC_HOOK_BLOCK, self._block, begin=DRVSEG * 16, end=DRVSEG * 16 + 0xFFFF)
        for seg in self.WATCH:
            self.watch(seg)
        # a run unpacked into CS:0060 longer than 576 bytes overwrites the driver's code
        mu.hook_add(UC_HOOK_MEM_WRITE, self._selfmod, begin=DRVSEG * 16 + CODE[0], end=DRVSEG * 16 + CODE[1] - 1)
        self.dyn = []
        self.error = None
        self.mode_sets = 0

    def watch(self, seg, dynamic=False):
        a = seg * 16 + 0xFFFF
        h = self.mu.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self._edge, begin=a, end=a)
        if dynamic:
            self.dyn.append(h)

    def unwatch(self):
        for h in self.dyn:
            self.mu.hook_del(h)
        self.dyn = []

    def _edge(self, mu, access, address, size, value, user):
        if size > 1:
            self.error = 'word access at offset FFFFh (%05X)' % address
            mu.emu_stop()

    def _selfmod(self, mu, access, address, size, value, user):
        self.error = 'writes into its own code (CS:%04X)' % (address - DRVSEG * 16)
        mu.emu_stop()

    def _block(self, mu, address, size, user):
        o = address - DRVSEG * 16
        self.covered[o:o + size] = b'\1' * size

    def _intr(self, mu, intno, user):
        ax = mu.reg_read(UC_X86_REG_AX)
        if intno == 0x10 and ax == 0x0013:
            self.write(0xA0000, bytes(0x10000))
            self.mode_sets += 1
        else:
            self.error = 'unexpected int %02Xh AX=%04X' % (intno, ax)
            mu.emu_stop()

    def read(self, lin, n):
        return bytes(self.mu.mem_read(lin, n))

    def write(self, lin, data):
        self.mu.mem_write(lin, bytes(data))

    def call(self, entry, words=(), regs=None, limit=LIMIT):
        """words: the stack arguments in memory order (the first argument first)."""
        mu = self.mu
        sp = 0xFFF0 - 2 * len(words)
        self.write(DGROUP * 16 + sp, b''.join(struct.pack('<H', w & 0xFFFF) for w in words))
        sp -= 4
        self.write(DGROUP * 16 + sp, struct.pack('<HH', RET_TRAP, STUBSEG))
        init = {UC_X86_REG_CS: DRVSEG, UC_X86_REG_DS: DGROUP, UC_X86_REG_SS: DGROUP,
                UC_X86_REG_ES: DGROUP, UC_X86_REG_SP: sp, UC_X86_REG_EFLAGS: 0x0002,
                UC_X86_REG_AX: 0, UC_X86_REG_BX: 0, UC_X86_REG_CX: 0, UC_X86_REG_DX: 0,
                UC_X86_REG_SI: 0, UC_X86_REG_DI: 0, UC_X86_REG_BP: 0}
        for r, v in (regs or {}).items():
            init[r] = v & 0xFFFF
        for r, v in init.items():
            mu.reg_write(r, v)
        self.error = None
        try:
            mu.emu_start(DRVSEG * 16 + entry, STUBSEG * 16 + RET_TRAP, count=limit)
        except Exception as e:  # unicorn errors: invalid memory etc.
            self.error = 'unicorn: %s' % e
        if self.error:
            raise Invalid(self.error)
        if mu.reg_read(UC_X86_REG_CS) != STUBSEG or mu.reg_read(UC_X86_REG_IP) != RET_TRAP:
            raise Invalid('did not return within %d instructions' % limit)
        if mu.reg_read(UC_X86_REG_SP) != sp + 4:
            raise RuntimeError('entry %02Xh: stack not balanced (RETF n?)' % entry)


# ---- the C ----

class CSurface(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('segment', ctypes.c_uint16), ('pScanlines', VP),
                ('right', I16), ('left', I16), ('top', I16), ('bottom', I16)]


class CText(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('surface', VP), ('curX', I16), ('curY', I16), ('font', VP),
                ('fgColor', ctypes.c_int8), ('bgColor', ctypes.c_int8),
                ('buffer', VP), ('bufPtr', VP), ('justify', ctypes.c_int8)]


class CDrv:
    def __init__(self, libpath):
        self.lib = lib = ctypes.CDLL(libpath)
        lib.shim_arena.restype = VP
        lib.ugfx_segment.restype = VP
        lib.dosmem_init()
        lib.shim_install_hook()
        assert lib.shim_surface_size() == ctypes.sizeof(CSurface), 'struct tSurface layout'
        assert lib.shim_text_size() == ctypes.sizeof(CText), 'struct tText layout'
        self.arena = lib.shim_arena()
        self.seg = lib.ugfx_segment()
        self.fns = {}
        for name, args in PROTOS.items():
            f = getattr(lib, 'UDRV_' + name)
            f.restype = None
            f.argtypes = args
            self.fns[name] = f

    def ptr(self, lin):
        return self.arena + lin

    def read(self, lin, n):
        return ctypes.string_at(self.arena + lin, n)

    def write(self, lin, data):
        data = bytes(data)
        ctypes.memmove(self.arena + lin, data, len(data))


# ---- what both sides are given ----

class Surf:
    """A struct tSurface: the pixels' segment, the scanline table (linear), the window."""
    def __init__(self, seg, scan, right, left, top, bottom, name=''):
        self.seg, self.scan = seg, scan
        self.right, self.left, self.top, self.bottom = right, left, top, bottom
        self.name = name

    def win(self, right, left, top, bottom, name=None):
        return Surf(self.seg, self.scan, right, left, top, bottom, name or self.name + "'")

    def __repr__(self):
        return '%s{%04X scan %05X r %d l %d t %d b %d}' % (self.name, self.seg, self.scan, self.right,
                                                           self.left, self.top, self.bottom)


def far_words(lin, off=None):
    """a far pointer to linear lin: (offset, segment), normalised unless off is given"""
    if off is None:
        off = lin & 15
    return [off, (lin - off) >> 4]


class Mismatch(Exception):
    pass


class Session:
    def __init__(self, libpath, image, rnd):
        self.emu = Emu(image)
        self.c = CDrv(libpath)
        self.rnd = rnd
        self.count = {}
        self.skipped = {}
        self.reasons = {}
        self.history = []
        self.keep = []
        self.snap = None
        self.shape_lin = SHAPE_LIN
        self.shape_n = 0
        self.shape_data = b''

    # -- memory --
    def put(self, lin, data):
        self.emu.write(lin, data)
        self.c.write(lin, data)

    def snapshot(self):
        self.snap = self.emu.read(0, 0x110000)

    def rollback(self):
        self.emu.write(0, self.snap)

    def compare(self, what):
        e, c = self.emu, self.c
        for lo, hi in CMP_RANGES:
            a, b = e.read(lo, hi - lo), c.read(lo, hi - lo)
            if a != b:
                diffs = [lo + i for i in range(hi - lo) if a[i] != b[i]]
                raise Mismatch('%s: memory differs at %d bytes, first %s' % (
                    what, len(diffs), ', '.join('%05X: orig %02X C %02X' % (d, a[d - lo], b[d - lo])
                                                for d in diffs[:8])))
        drv_e = e.read(DRVSEG * 16, 0x10000)
        drv_c = ctypes.string_at(c.seg, 0x10000)
        for lo, hi in DRV_CMP:
            if drv_e[lo:hi] != drv_c[lo:hi]:
                diffs = [i for i in range(lo, hi) if drv_e[i] != drv_c[i]]
                raise Mismatch('%s: driver segment differs at %s' % (
                    what, ', '.join('CS:%04X orig %02X C %02X' % (d, drv_e[d], drv_c[d]) for d in diffs[:8])))
        self.snap = e.read(0, 0x110000)

    # -- the arguments --
    def esurf(self, s, slot):
        o = DG_SURF[slot]
        so, sg = far_words(s.scan)
        self.emu.write(DGROUP * 16 + o, struct.pack('<HHHhhhh', s.seg, so, sg, s16(s.right), s16(s.left),
                                                    s16(s.top), s16(s.bottom)))
        return o

    def csurf(self, s):
        cs = CSurface(s.seg & 0xFFFF, self.c.ptr(s.scan), s16(s.right), s16(s.left), s16(s.top), s16(s.bottom))
        self.keep.append(cs)
        return ctypes.addressof(cs)

    # -- calling both --
    def run(self, name, emu_fn, c_fn, desc, shape_seg=None):
        self.history.append(desc)
        if len(self.history) > 30:
            del self.history[0]
        self.keep = []
        if shape_seg is not None and shape_seg not in Emu.WATCH:
            self.emu.watch(shape_seg, dynamic=True)
        try:
            emu_fn()
        except Invalid as e:
            self.rollback()
            self.skipped[name] = self.skipped.get(name, 0) + 1
            why = str(e).split(' (')[0].split(' within')[0]
            self.reasons[why] = self.reasons.get(why, 0) + 1
            self.history[-1] += '  [skipped: %s]' % e
            return False
        finally:
            self.emu.unwatch()
        c_fn()
        self.compare(desc)
        self.count[name] = self.count.get(name, 0) + 1
        return True

    def load_shapes(self, data, lin=SHAPE_LIN):
        assert lin + len(data) <= 0xA0000
        self.put(lin, data)
        self.shape_lin = lin
        self.shape_data = data
        self.shape_n = (struct.unpack_from('<I', data, 4)[0] - 4) // 4
        self.snapshot()

    def shape_box(self, index):
        o = struct.unpack_from('<I', self.shape_data, 4 + 4 * index)[0]
        return struct.unpack_from('<hhhh', self.shape_data, o)   # right, left, up, down

    def shapes_ptr(self, denorm=False):
        """the shapes far pointer for the original (sometimes not normalised) and the C"""
        lin = self.shape_lin
        if denorm:
            off = self.rnd.choice([lin & 15, (lin & 0xFFFF) | (lin & 15), 0x8000 + (lin & 15)])
            if off > lin:
                off = lin & 15
        else:
            off = lin & 15
        return far_words(lin, off), self.c.ptr(lin)

    def shape_seg(self, index):
        """the segment the original normalises shape index to (for the FFFFh watch)"""
        bx = ((index << 2) + 4) & 0xFFFF
        lo = self.shape_lin & 15
        a = (self.shape_lin - lo) + ((lo + bx) & 0xFFFF)
        d = struct.unpack('<I', self.emu.read(a, 4))[0]
        return ((self.shape_lin + d) >> 4) & 0xFFFF

    # -- the entries --
    def d0(self):
        cbuf = (ctypes.c_uint16 * 200)()
        before = self.c.lib.shim_mode_sets()

        def emu():
            self.emu.write(DGROUP * 16 + DG_SCAN0, bytes(400))
            self.emu.call(0x00, [DG_SCAN0])

        def c():
            self.c.fns['0'](ctypes.addressof(cbuf))
            got_e = self.emu.read(DGROUP * 16 + DG_SCAN0, 400)
            if got_e != bytes(cbuf):
                raise Mismatch('entry 0: scanline tables differ')
            if self.c.lib.shim_mode_sets() - before != 1:
                raise Mismatch('entry 0: the mode hook was not called once')
        return self.run('0', emu, c, 'UDRV_0(table)')

    def d3(self, s, x, y, index, denorm=False):
        w, cp = self.shapes_ptr(denorm)

        def emu():
            self.emu.call(0x03, [self.esurf(s, 0), x, y] + w + [index])

        def c():
            self.c.fns['3'](self.csurf(s), s16(x), s16(y), cp, s16(index))
        return self.run('3', emu, c, 'UDRV_3(%r, %d, %d, shapes %04X:%04X, %d)' % (s, s16(x), s16(y), w[1], w[0], s16(index)),
                        shape_seg=self.shape_seg(index))

    def d69(self, which, s, save_lin, x, y, index):
        w, cp = self.shapes_ptr()
        b = far_words(save_lin)
        e = 0x06 if which == '6' else 0x09

        def emu():
            self.emu.call(e, [self.esurf(s, 0)] + b + [x, y] + w + [index])

        def c():
            self.c.fns[which](self.csurf(s), self.c.ptr(save_lin), s16(x), s16(y), cp, s16(index))
        return self.run(which, emu, c, 'UDRV_%s(%r, buf %05X, %d, %d, shapes, %d)' % (which, s, save_lin, s16(x), s16(y), s16(index)),
                        shape_seg=self.shape_seg(index))

    def dc(self, s, color):
        def emu():
            self.emu.call(0x0C, [self.esurf(s, 0), color])

        def c():
            self.c.fns['0xc'](self.csurf(s), s16(color))
        return self.run('0xc', emu, c, 'UDRV_0xc(%r, %d)' % (s, color))

    def df(self, src, dst):
        def emu():
            self.emu.call(0x0F, [self.esurf(src, 0), self.esurf(dst, 1)])

        def c():
            self.c.fns['0xf'](self.csurf(src), self.csurf(dst))
        return self.run('0xf', emu, c, 'UDRV_0xf(%r, %r)' % (src, dst))

    def d12(self, s, font_lin, curX, curY, fg, bg, ch, height, width, y2):
        ctext = []

        def emu():
            so = self.esurf(s, 0)
            fo, fs = far_words(font_lin)
            self.emu.write(DGROUP * 16 + DG_TEXT, struct.pack('<HhhHHBBHHB', so, s16(curX), s16(curY), fo, fs,
                                                              fg & 0xFF, bg & 0xFF, 0x1234, 0x1235, 0))
            self.emu.call(0x12, regs={UC_X86_REG_AX: ch, UC_X86_REG_BX: DG_TEXT, UC_X86_REG_CX: height,
                                      UC_X86_REG_DX: width, UC_X86_REG_SI: y2})

        def c():
            t = CText(self.csurf(s), s16(curX), s16(curY), self.c.ptr(font_lin), s16(fg << 8) >> 8,
                      s16(bg << 8) >> 8, 0x1234, 0x1235, 0)
            ctext.append(t)
            self.c.fns['0x12'](ctypes.byref(t), s16(ch), s16(height), s16(width), s16(y2))
            e = struct.unpack('<HhhHHBBHHB', self.emu.read(DGROUP * 16 + DG_TEXT, 17))
            if e[1] != t.curX or e[2] != t.curY or (e[5], e[6]) != (fg & 0xFF, bg & 0xFF):
                raise Mismatch('entry 12h: tText differs: orig curX %d curY %d, C curX %d curY %d' % (
                    e[1], e[2], t.curX, t.curY))
        return self.run('0x12', emu, c, 'UDRV_0x12(text{%r font %05X x %d y %d fg %02X bg %02X}, ch %02X, h %d, w %d, y2 %d)' % (
            s, font_lin, s16(curX), s16(curY), fg & 0xFF, bg & 0xFF, ch, height, width, s16(y2)))

    def d15(self, s):
        def emu():
            self.emu.call(0x15, [self.esurf(s, 0)])

        def c():
            self.c.fns['0x15'](self.csurf(s))
        return self.run('0x15', emu, c, 'UDRV_0x15(%r)' % s)

    def d18(self, s, x, y, color):
        def emu():
            self.emu.call(0x18, [self.esurf(s, 0), x, y, color])

        def c():
            self.c.fns['0x18'](self.csurf(s), s16(x), s16(y), s16(color))
        return self.run('0x18', emu, c, 'UDRV_0x18(%r, %d, %d, %d)' % (s, s16(x), s16(y), color))


# ---- the test data ----

def scan_table(rows, width, base):
    return b''.join(struct.pack('<H', (base + y * width) & 0xFFFF) for y in range(rows))


class World:
    """the surfaces U.EXE uses, and the memory they live in"""
    def __init__(self, ses, rnd):
        self.ses, self.rnd = ses, rnd
        put = ses.put
        put(STUBSEG * 16 + RET_TRAP, b'\xF4')
        # scanline tables: the screen (y * 320), off-screen buffers (SURF_alloc's: 8 + y * w),
        # then random words, read by rows outside the tables
        t_screen, t_back, t_work, t_odd = SCAN_LIN, SCAN_LIN + 0x200, SCAN_LIN + 0x400, SCAN_LIN + 0x608
        put(SCAN_LIN, bytes(rnd.getrandbits(8) & 0xFE for _ in range(0x10000)))   # even garbage offsets
        put(t_screen, scan_table(200, 320, 0))
        put(t_back, scan_table(200, 320, 8))
        put(t_work, scan_table(100, 176, 8))
        put(t_odd, scan_table(64, 57, 3))
        self.screen = Surf(SCREEN, t_screen, 319, 0, 0, 199, 'screen')
        self.back = Surf(BUFSEG, t_back, 319, 0, 0, 199, 'back')
        self.work = Surf(BUFSEG, t_work, 175, 0, 0, 99, 'work')
        self.odd = Surf(BUFSEG, t_odd, 56, 0, 0, 63, 'odd')
        self.bases = [self.screen, self.back, self.work, self.odd]
        self.noise(0xA0000, 0x10000)
        self.noise(BUFSEG * 16, 0x10000)
        self.noise(SAVE_LIN, 0x10000)
        self.font = unz('U6.SET')
        put(FONT_LIN, self.font)
        f2 = bytearray(self.font)
        f2[3] = f2[2]
        put(FONT2_LIN, bytes(f2))
        self.font2 = bytes(f2)
        ses.snapshot()

    def noise(self, lin, n):
        self.ses.put(lin, bytes(self.rnd.getrandbits(8) for _ in range(n)))

    def coord(self, lo, hi):
        r = self.rnd.random()
        if r < 0.03:
            return self.rnd.getrandbits(16)
        if r < 0.06:
            return self.rnd.choice([0x7FFF, 0x8000, 0x7FF0, 0x8010, 0xFFFF, 0])
        return self.rnd.randint(lo, hi) & 0xFFFF

    def window(self, base=None, sane=True):
        """a random window on a base surface: partly or wholly outside it, sometimes"""
        b = base or self.rnd.choice(self.bases)
        r = self.rnd.random()
        if r < 0.5:
            l = self.rnd.randint(b.left, max(b.left, b.right))
            t = self.rnd.randint(b.top, max(b.top, b.bottom))
            rr = self.rnd.randint(l, max(l, b.right))
            bb = self.rnd.randint(t, max(t, b.bottom))
        elif r < 0.8:
            l = self.rnd.randint(-40, max(-40, b.right + 20))
            t = self.rnd.randint(-20, max(-20, b.bottom + 20))
            rr = l + self.rnd.randint(0, 120)
            bb = t + self.rnd.randint(0, 80)
        else:
            l = self.rnd.randint(-400, 700)
            t = self.rnd.randint(-300, 500)
            rr = l + self.rnd.randint(0, 50)
            bb = t + self.rnd.randint(0, 50)
        if not sane and self.rnd.random() < 0.3:   # empty: one past
            if self.rnd.random() < 0.5:
                rr = l - 1
            else:
                bb = t - 1
        return b.win(rr, l, t, bb)


# ---- scenarios ----

def scenario_shapes(ses, w, rnd, quick):
    """every shape of every shape file, unclipped and clipped, its box saved and restored"""
    for name in shape_files():
        data = unz(name)
        lin = SHAPE_LIN + rnd.choice([0, 0, 3, 8, 15])
        if lin + len(data) > 0xA0000:
            lin = SHAPE_LIN
        ses.load_shapes(data, lin)
        n = ses.shape_n
        idx = list(range(n))
        if quick:
            idx = sorted(rnd.sample(idx, min(n, 8)))
        for i in idx:
            r, l, u, d = ses.shape_box(i)
            # natural: the hot spot so that the box starts at the window's corner (unclipped
            # when it fits)
            ses.d3(w.back, l, u, i)
            ses.d3(w.screen, l + rnd.randint(0, max(0, 319 - r - l)), u + rnd.randint(0, max(0, 199 - u - d)), i,
                   denorm=rnd.random() < 0.3)
            for _ in range(1 if quick else 3):
                ses.d3(w.screen, rnd.randint(-r - 20, 340 + l), rnd.randint(-d - 20, 220 + u), i)
            ses.d3(w.window(), w.coord(-100, 400), w.coord(-100, 300), i)
            # the box: save, scribble, restore
            x, y = rnd.randint(-r - 10, 330 + l), rnd.randint(-d - 10, 210 + u)
            s = rnd.choice([w.screen, w.screen, w.back, w.window()])
            ses.d69('6', s, SAVE_LIN + rnd.randint(0, 15), x, y, i)
            ses.dc(w.window(s), rnd.getrandbits(8))
            ses.d69('9', s, SAVE_LIN + rnd.randint(0, 15), x, y, i)
        # what the bound check of 03h lets through: index == n (the first 4 bytes of shape 0
        # taken as an offset), negative indexes; and what it stops: n + 1
        for i in (n, n + 1, n + 2, 0xFFFF, 0x8000, rnd.getrandbits(16) | 0x8000):
            ses.d3(w.screen, rnd.randint(0, 319), rnd.randint(0, 199), i)


def scenario_pointer(ses, w, rnd, quick):
    """the mouse pointer as MOUSECUR.ASM draws it: save, draw, restore, at every edge"""
    ses.load_shapes(unz('INTRO.PTR'))
    xs = [-20, -13, -12, -7, -1, 0, 1, 5, 150, 306, 310, 314, 318, 319, 320, 326, 340]
    ys = [-20, -14, -13, -7, -1, 0, 1, 5, 100, 186, 190, 195, 198, 199, 200, 213, 230]
    for i in range(ses.shape_n):
        pts = [(x, y) for x in xs for y in ys]
        if quick:
            pts = rnd.sample(pts, 20)
        for x, y in pts:
            buf = SAVE_LIN + rnd.randint(0, 15)
            ses.d69('6', w.screen, buf, x, y, i)
            ses.d3(w.screen, x, y, i)
            ses.d69('9', w.screen, buf, x, y, i)


def scenario_random(ses, w, rnd, ops, files):
    names = list(files)
    for k in range(ops):
        if k % 400 == 0:
            ses.load_shapes(unz(rnd.choice(names)), SHAPE_LIN + rnd.randint(0, 15))
        r = rnd.random()
        n = ses.shape_n
        if r < 0.25:
            i = rnd.randrange(n) if rnd.random() < 0.95 else rnd.choice([n, n + 1, -1])
            ses.d3(w.window(sane=False) if rnd.random() < 0.5 else rnd.choice(w.bases),
                   w.coord(-350, 650), w.coord(-250, 450), i & 0xFFFF, denorm=rnd.random() < 0.2)
        elif r < 0.40:
            i = rnd.randrange(n)
            s = w.window() if rnd.random() < 0.5 else rnd.choice(w.bases)
            ses.d69(rnd.choice('69'), s, SAVE_LIN + rnd.randint(0, 15), w.coord(-350, 650), w.coord(-250, 450), i)
        elif r < 0.55:
            ses.dc(w.window(sane=False), rnd.getrandbits(16))
        elif r < 0.70:
            a = w.window()
            if rnd.random() < 0.3:
                b = a.win(a.right + rnd.randint(-3, 3), a.left + rnd.randint(-3, 3), a.top + rnd.randint(-3, 3),
                          a.bottom + rnd.randint(-3, 3))   # overlapping, on the same pixels
                if b.right < b.left or b.bottom < b.top:
                    b = a
            else:
                b = w.window()
            ses.df(a, b)
        elif r < 0.90:
            text_op(ses, w, rnd)
        elif r < 0.98:
            s = w.window(sane=False) if rnd.random() < 0.5 else rnd.choice(w.bases)
            ses.d18(s, w.coord(s.left - 3, s.right + 3), w.coord(s.top - 3, s.bottom + 3), rnd.getrandbits(16))
        else:
            ses.d15(rnd.choice(w.bases))


def text_op(ses, w, rnd, s=None):
    s = s or (w.window() if rnd.random() < 0.6 else rnd.choice([w.screen, w.back, w.work]))
    font_lin, font = (FONT_LIN, w.font) if rnd.random() < 0.8 else (FONT2_LIN, w.font2)
    ch = rnd.getrandbits(8)
    height = struct.unpack_from('<H', font, 0)[0]
    width = font[4 + ch]
    r = rnd.random()
    if r < 0.08:
        height = rnd.randint(1, 12)
    elif r < 0.10:
        width = rnd.randint(0, 12)
    elif r < 0.105:
        height = 0                       # 65536 rows
        width = rnd.randint(0, 2)
    curX = rnd.randint(s.left, max(s.left, s.right)) if rnd.random() < 0.9 else w.coord(-50, 400)
    if rnd.random() < 0.3:
        curX = s.right - rnd.randint(0, 10)   # the clamp at the right
    curY = rnd.randint(s.top, max(s.top, s.bottom)) if rnd.random() < 0.9 else rnd.randint(-10, 210)
    y2 = (2 * curY) & 0xFFFF if rnd.random() < 0.95 else rnd.getrandbits(16)
    c = rnd.random()
    if c < 0.3:
        fg, bg = font[2], font[3]          # copied as it is
    elif c < 0.4:
        fg, bg = font[2], rnd.getrandbits(8)
    elif c < 0.5:
        fg, bg = rnd.getrandbits(8), font[3]
    else:
        fg, bg = rnd.getrandbits(8), rnd.getrandbits(8)
    ses.d12(s, font_lin, curX & 0xFFFF, curY & 0xFFFF, fg, bg, ch, height, width, y2)


def scenario_text(ses, w, rnd, quick):
    """every character of the font, at the console's place and elsewhere"""
    for ch in range(256):
        for fg, bg in ((w.font[2], w.font[3]), (0x48, 0x31), (w.font[3], w.font[2])):
            ses.d12(w.screen, FONT_LIN, 8 + (ch % 38) * 8, 8 + (ch // 38) * 9, fg, bg, ch,
                    struct.unpack_from('<H', w.font, 0)[0], w.font[4 + ch], 2 * (8 + (ch // 38) * 9))
            if quick:
                break
    for _ in range(100 if quick else 600):
        text_op(ses, w, rnd)


def synth_file(shapes):
    """a shape file: size dword, offset table, shapes (box + run words + 0)"""
    n = len(shapes)
    out = bytearray(4 + 4 * n)
    for i, (box, body) in enumerate(shapes):
        struct.pack_into('<I', out, 4 + 4 * i, len(out))
        out += struct.pack('<hhhh', *box) + body
    struct.pack_into('<I', out, 0, len(out))
    return bytes(out)


def rle_run(x, y, chunks):
    """chunks: ('c', n, color) or ('l', bytes)"""
    body = bytearray()
    total = 0
    for ch in chunks:
        if ch[0] == 'c':
            body += bytes([ch[1] << 1 | 1, ch[2]])
            total += ch[1]
        else:
            body += bytes([len(ch[1]) << 1]) + ch[1]
            total += len(ch[1])
    return struct.pack('<Hhh', total << 1 | 1, x, y) + body


def raw_run(x, y, data):
    return struct.pack('<Hhh', len(data) << 1, x, y) + data


def scenario_synthetic(ses, w, rnd):
    """the odd cases the game's files do not have"""
    def chunks(total):
        out, left = [], total
        while left:
            k = min(left, rnd.randint(1, 127))
            out.append(('c', k, rnd.getrandbits(8)) if rnd.random() < 0.5 else ('l', bytes(rnd.getrandbits(8) for _ in range(k))))
            left -= k
        return out
    shapes = []
    # 0: long RLE runs (up to 576 = the room from CS:0060 to the code at CS:02A0); clipped at
    # an end, they are unpacked into CS:0060 and overwrite the color table at CS:01A0
    body = b''.join(rle_run(-300 + 3 * k, k, chunks(rnd.randint(330, 576))) for k in range(20)) + b'\0\0'
    shapes.append(((300, 300, 0, 19), body))
    # 1: a run word of 1 (RLE, no pixels): the unclipped path reads one chunk byte (0: none),
    # the clipped path takes it for the end of the shape
    body = raw_run(0, 0, b'\x11\x22\x33') + struct.pack('<Hhh', 1, 0, 1) + b'\x00' + raw_run(0, 2, b'\x44\x55') + b'\0\0'
    shapes.append(((4, 0, 0, 3), body))
    # 2: a box that lies: runs outside it are drawn without clipping when the box fits
    body = raw_run(-30, -5, bytes(range(60))) + rle_run(10, 8, [('c', 50, 0x77)]) + b'\0\0'
    shapes.append(((2, 2, 2, 2), body))
    # 3: odd and even lengths, chunks of every kind, overlapping rows
    body = b''.join(raw_run(rnd.randint(-10, 10), rnd.randint(-10, 10), bytes(rnd.getrandbits(8) for _ in range(rnd.randint(1, 30))))
                    if rnd.random() < 0.5 else rle_run(rnd.randint(-10, 10), rnd.randint(-10, 10), chunks(rnd.randint(1, 40)))
                    for _ in range(40)) + b'\0\0'
    shapes.append(((50, 50, 20, 20), body))
    # 4: negative extents (a box with right < left): widths that wrap in 06h/09h
    body = raw_run(0, 0, b'\x01\x02') + b'\0\0'
    shapes.append(((-3, 1, 0, 0), body))
    ses.load_shapes(synth_file(shapes), SHAPE_LIN + 5)
    for _ in range(300):
        i = rnd.randrange(len(shapes))
        s = rnd.choice([w.screen, w.back, w.window(), w.window()])
        ses.d3(s, w.coord(-350, 650), w.coord(-50, 250), i)
        if rnd.random() < 0.3:
            ses.d69(rnd.choice('69'), s, SAVE_LIN + rnd.randint(0, 15), w.coord(-100, 400), w.coord(-100, 300), i)
        if rnd.random() < 0.3:
            text_op(ses, w, rnd)   # through the color table as 03h left it
    # shape 0 clipped on the left and on the right of the screen, then text
    for x in (0, 40, 300, 330, 620):
        ses.d3(w.screen, x, 50, 0)
        for _ in range(5):
            text_op(ses, w, rnd, w.screen)
    # every clipping offset at both ends of a window: one RLE run and one literal run of
    # each length 1..24 (shape 2L-2: RLE, 2L-1: literal), their first pixel at the hot spot
    shapes = []
    for n in range(1, 25):
        shapes.append(((n - 1, 0, 0, 0), rle_run(0, 0, chunks(n)) + b'\0\0'))
        shapes.append(((n - 1, 0, 0, 0), raw_run(0, 0, bytes(rnd.getrandbits(8) for _ in range(n))) + b'\0\0'))
    ses.load_shapes(synth_file(shapes), SHAPE_LIN + 9)
    for s in (w.screen, w.screen.win(200, 100, 20, 30), w.work):
        for i in range(len(shapes)):
            n = i // 2 + 1
            for k in range(-2, 3):
                ses.d3(s, s.left + k - n + 1, s.top, i)      # the end at left + k
                ses.d3(s, s.left + k, s.bottom, i)           # the start at left + k
                ses.d3(s, s.right + k - n + 1, s.top, i)     # the end at right + k
                ses.d3(s, s.right + k, s.bottom, i)          # the start at right + k
                ses.d3(s, s.left + k, s.top - 1 + (k & 1) * (s.bottom - s.top + 2), i)   # a row outside


def coverage(emu, image_len):
    lo, hi = CODE
    hi = min(hi, image_len)
    missing = []
    o = lo
    while o < hi:
        if not emu.covered[o]:
            start = o
            while o < hi and not emu.covered[o]:
                o += 1
            missing.append((start, o))
        o += 1
    done = sum(1 for o in range(lo, hi) if emu.covered[o])
    return done, hi - lo, missing


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ops', type=int, default=4000, help='random calls (default 4000)')
    ap.add_argument('--seed', type=int, default=6)
    ap.add_argument('--quick', action='store_true', help='a sample of the shapes and characters')
    args = ap.parse_args()
    rnd = random.Random(args.seed)
    image = unz('MCGADRV.BIN')
    print('MCGADRV.BIN: %d bytes unpacked' % len(image))
    with tempfile.TemporaryDirectory() as tmp:
        lib = build_lib(tmp)
        ses = Session(lib, image, rnd)
        w = World(ses, rnd)
        try:
            ses.d0()
            w.noise(0xA0000, 0x10000)
            ses.snapshot()
            scenario_pointer(ses, w, rnd, args.quick)
            scenario_text(ses, w, rnd, args.quick)
            scenario_shapes(ses, w, rnd, args.quick)
            scenario_random(ses, w, rnd, args.ops // (4 if args.quick else 1), shape_files())
            scenario_synthetic(ses, w, rnd)
            ses.d15(w.screen)
        except Mismatch as e:
            print('MISMATCH: %s' % e)
            print('last calls:')
            for h in ses.history[-12:]:
                print('  ' + h)
            return 1
    total = sum(ses.count.values())
    print('calls compared: %d (%s)' % (total, ', '.join('%s: %d' % (k, v) for k, v in sorted(ses.count.items()))))
    if ses.skipped:
        print('skipped (not valid tests): %s; %s' % (', '.join('%s: %d' % kv for kv in sorted(ses.skipped.items())),
                                                  ', '.join('%s: %d' % kv for kv in sorted(ses.reasons.items()))))
    done, n, missing = coverage(ses.emu, len(image))
    print('driver code executed: %d of %d bytes%s' % (done, n, '' if not missing else
          ' (not: %s)' % ', '.join('%04X-%04X' % m for m in missing)))
    print('all identical')
    return 0


if __name__ == '__main__':
    sys.exit(main())
