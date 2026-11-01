#!/usr/bin/env python3
"""Differential test of port/u/osilib (U.EXE's OSI library in C) against the original.

The original routines run from U.EXE itself (the game folder's, loaded with its
relocations, DS = DGROUP) in unicorn, with the graphics driver MCGADRV.BIN (the game
folder's, unpacked) at its own segment for the entries the library calls (3, 6, 9, 0Ch,
0Fh, 12h), and stand-ins for what is outside the library: Turbo C's heap functions
(malloc, calloc, free, farmalloc, farcalloc, farfree: patched to hand out what the C side
got, so that both sides use the same addresses), getch and kbhit, the DOS file calls on
the game folder (int 21h), the mouse driver (int 33h), the BIOS tick (int 1Ah), the VGA
DAC and the retrace bit (ports 3C7h-3C9h, 3DAh). The C side is the library compiled with
U's force-includes into a shared library (with port/u/ugfx.c, the port/sys files it
uses and the platform stand-ins of osi_stubs.c, uosi_shim.c), driven through ctypes.
Both get the same memory (the same contents at the same linear addresses: the screen at
A000:0000, surfaces, the font U6.SET, the pointer shapes INTRO.PTR); after each operation
the pixels, the structures, the buffers, the DAC and the results are compared.

  TEXT    TXT_puts in the 4 justifications (and 5: as 3), TXT_putc, TXT_printf, TXT_sprintf,
          TXT_sappend (every format), TXT_clear, TXT_clearEos, clreol, gotoxy,
          TXT_scrollUp/Down: on the screen and on a far surface, random windows, colors
  SURF    SURF_alloc (the allocations asked, the table), SURF_free (what is freed),
          SURF_share
  DRAW    DRW_line (clipping, negative and huge coordinates), DRW_getPixel
  MOUSE   MOUSE_INIT/NOINIT/SERVE/SHOW/HIDE/POINTER/DRAW/SAVE_BG/IN_AREA/IN_RECT,
          SHAPE_IN_RECT on INTRO.PTR: results, screen, save buffer, the structure, onMove
  FADE    FADE_step; FADE_in and FADE_out: the DAC at every vertical retrace, the screen
  DISS    DIS_dissolve (MCGA), DIS_dissolveCGA, DIS_dissolveTDY: the target memory at every
          wait and at the end, the number of waits; DIS_farcpy
  LZW     LZW_load of every LZW file of the game folder: data, size asked, result
  LOAD    LOAD_near, LOAD_far: contents, results, sizes asked
  PAL     PAL_set, PAL_get
then unit tests of what depends on the platform (simulated clock, keys, mouse):
  TIMER   TIM_calibrate, the callbacks at poll points, TIM_delay's duration
  VSYNC   VID_waitRetrace's 70 Hz pacing, VID_inRetrace after it
  KEYS    kbhit, getch (extended keys)
  MOUSEP  MOUSE_INIT/SERVE on the platform's mouse
  FILES   OSI_open/read/close and U's own error state (GAME's untouched)

  python3 port/tests/uosi_diff.py [--quick] [--seed N] [test names...]

Exit status 0 when everything matched.
"""
import ctypes
import os
import random
import re
import struct
import subprocess
import sys
import tempfile
import zlib

from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INTR, UC_HOOK_INSN, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX, UC_X86_REG_DX,
                               UC_X86_REG_SP, UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES,
                               UC_X86_REG_SS, UC_X86_REG_EFLAGS, UC_X86_REG_IP,
                               UC_X86_INS_IN, UC_X86_INS_OUT)

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
ROOT = os.path.dirname(PORT)
GAMEDATA = os.environ.get('U6_GAMEDATA', '/home/claude/gamedata')
UEXE = os.environ.get('U6_UEXE', os.path.join(GAMEDATA, 'U.EXE'))
sys.path.insert(0, HERE)
import u6lzw  # noqa: E402

QUICK = '--quick' in sys.argv

LOAD = 0x0800          # U.EXE's load segment in the emulated machine
DGROUP = 0x1499        # its data segment, relative
DRVSEG = 0x0600        # MCGADRV.BIN
STUBSEG = 0x0700       # callback stubs (onMove, ...)
STOP = 0x00500         # a HLT: the far return address of every call
TRAP = 0xF0            # INT F0h: the heap/conio stand-ins

# DS offsets of the test's own data in DGROUP (above U's _BSS, which ends at 683Eh)
DS_SCAN = 0x7000       # the screen's scanline table (scanlineOfs stands here)
DS_SURF = 0x7200       # surfaces
DS_TEXT = 0x7300       # text windows
DS_MOUSE = 0x7340
DS_STR = 0x7400        # strings (1 KB)
DS_BUF = 0x7800        # the text window's buffer (1 KB)
DS_VEC = 0x7C00        # FADE_step's vectors
NEAR_HEAP = (0x8000, 0xF000)   # the stack is above, at the top of DGROUP

# linear addresses of the shared far memory (same contents on both sides)
FONT_LIN = 0x60000
SHAPES_LIN = 0x64000
SAVE_LIN = 0x68000
FARSCAN_LIN = 0x6F000   # a far surface's scanline table
FARPIX_SEG = 0x7000     # its pixels
SCREEN_SEG = 0xA000

# D_ offsets of the library's data
D_PTEXT = 0x45B2
D_PMOUSE = 0x459E
D_MOUSESEM = 0x45A0
D_DRVTABLE = 0x45BC
D_GFXDRV = 0x4A34
D_FADESTART = 0x45E2


def w16(v):
    return v & 0xFFFF


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def gamefile(name):
    want = name.lower()
    for f in os.listdir(GAMEDATA):
        if f.lower() == want:
            return os.path.join(GAMEDATA, f)
    return None


def unz(name):
    return bytes(u6lzw.decompress(open(gamefile(name), 'rb').read())[0])


# ---------------------------------------------------------------- symbols, image

def symbols():
    code = {}
    out = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'u', 'usym.py'), 'list'],
                         capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        f = line.split()
        if len(f) < 2:
            continue
        m = re.fullmatch(r'C_([0-9A-F]{4})_([0-9A-F]{4})', f[0])
        if m:
            code.setdefault(f[1].lstrip('_'), (int(m.group(1), 16), int(m.group(2), 16)))
    for line in open(os.path.join(ROOT, 'tools', 'u', 'usyms.txt')):
        f = line.split('#')[0].split()
        if len(f) == 2 and ':' in f[1]:
            s, o = f[1].split(':')
            code.setdefault(f[0].lstrip('_'), (int(s, 16), int(o, 16)))
    return code


# TXTSCRL.ASM's line copy (C_1317_00F7) is called near (PUSH AX; CALL) but returns with a
# RETF, which takes the pushed line number for CS: in DOS the first line a scroll copies
# crashes U.EXE. The port scrolls as intended; the test turns that RETF into a RET so that
# the scrolling can be compared.
SCROLL_RETF = (0x1317, 0x0145)


def load_image():
    d = open(UEXE, 'rb').read()
    h = struct.unpack_from('<14H', d)
    cblp, cp, crlc, cparhdr, lfarlc = h[1], h[2], h[3], h[4], h[12]
    size = (cp - 1) * 512 + cblp if cblp else cp * 512
    image = bytearray(d[cparhdr * 16:size])
    for i in range(crlc):
        off, seg = struct.unpack_from('<HH', d, lfarlc + 4 * i)
        p = seg * 16 + off
        v = struct.unpack_from('<H', image, p)[0]
        struct.pack_into('<H', image, p, w16(v + LOAD))
    p = SCROLL_RETF[0] * 16 + SCROLL_RETF[1]
    if image[p - 1:p + 1] != b'\x5a\xcb':
        raise RuntimeError('U.EXE: the RETF of TXTSCRL.ASM\'s line copy is not where expected')
    image[p] = 0xC3
    return image


def driver_image():
    p = gamefile('MCGADRV.BIN')
    if p:
        return unz('MCGADRV.BIN')
    return open('/home/claude/uport/mcgadrv.bin', 'rb').read()


# ---------------------------------------------------------------- the C side

OSILIB_C = ['surface.c', 'text.c', 'draw.c', 'mouse.c', 'video.c', 'fade.c', 'dissolve.c',
            'load.c', 'osifile.c', 'osigame.c', 'timer.c', 'conio.c']
PLAIN_C = ['u/ugfx.c', 'sys/dosmem.c', 'sys/dosfs.c', 'sys/osi.c', 'sys/lzw.c', 'sys/timer.c',
           'sys/dosint.c', 'gfx/vgadac.c', 'replay/replay.c', 'tests/osi_stubs.c',
           'tests/uosi_shim.c']


def build_lib(tmp):
    cc = os.environ.get('CC', 'cc')
    objs = []
    for f in sorted(os.listdir(os.path.join(PORT, 'u', 'osilib'))):
        if f.endswith('.c') and f not in OSILIB_C:
            OSILIB_C.append(f)
    for f in OSILIB_C:
        o = os.path.join(tmp, 'u_' + f + '.o')
        subprocess.check_call([cc, '-std=gnu89', '-fsigned-char', '-fwrapv', '-fno-strict-aliasing',
                               '-O1', '-g', '-fPIC', '-w',
                               '-include', os.path.join(PORT, 'include', 'compat.h'),
                               '-include', os.path.join(PORT, 'u', 'uport.h'),
                               '-I', os.path.join(ROOT, 'SRC_U'), '-I', os.path.join(PORT, 'u'),
                               '-I', os.path.join(PORT, 'include'), '-I', os.path.join(PORT, 'sys'),
                               '-I', os.path.join(PORT, 'platform'),
                               '-c', os.path.join(PORT, 'u', 'osilib', f), '-o', o])
        objs.append(o)
    for f in PLAIN_C:
        o = os.path.join(tmp, f.replace('/', '_') + '.o')
        subprocess.check_call([cc, '-std=c99', '-O1', '-g', '-fPIC', '-fsigned-char', '-fwrapv', '-w',
                               '-D_POSIX_C_SOURCE=200809L', '-D_DEFAULT_SOURCE',
                               '-c', os.path.join(PORT, f), '-o', o])
        objs.append(o)
    out = os.path.join(tmp, 'libuosi_test.so')
    subprocess.check_call([cc, '-shared', '-o', out] + objs + ['-lm'])
    return out


class CSurface(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('segment', ctypes.c_uint16), ('pScanlines', ctypes.c_void_p),
                ('right', ctypes.c_int16), ('left', ctypes.c_int16),
                ('top', ctypes.c_int16), ('bottom', ctypes.c_int16)]


class CText(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('surface', ctypes.c_void_p), ('curX', ctypes.c_int16), ('curY', ctypes.c_int16),
                ('font', ctypes.c_void_p), ('fgColor', ctypes.c_uint8), ('bgColor', ctypes.c_uint8),
                ('buffer', ctypes.c_void_p), ('bufPtr', ctypes.c_void_p), ('justify', ctypes.c_uint8)]


class CMouse(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('buttonL', ctypes.c_uint8), ('buttonR', ctypes.c_uint8), ('x', ctypes.c_int16),
                ('y', ctypes.c_int16), ('unused06', ctypes.c_int16), ('shapes', ctypes.c_void_p),
                ('shape', ctypes.c_int16), ('surface', ctypes.c_void_p), ('saveBuf', ctypes.c_void_p),
                ('onMove', ctypes.c_void_p), ('installed', ctypes.c_uint8)]


WAITCB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_uint64)


class CSide:
    def __init__(self, path, overlay):
        self.lib = lib = ctypes.CDLL(path)
        lib.stub_quiet_log(1)
        lib.shim_init.restype = ctypes.POINTER(ctypes.c_uint8)
        self.arena = ctypes.addressof(lib.shim_init().contents)
        lib.dosfs_init.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        if lib.dosfs_init(GAMEDATA.encode(), overlay.encode()) != 0:
            raise RuntimeError('dosfs_init failed')
        lib.shim_linear.argtypes = [ctypes.c_void_p]
        lib.shim_linear.restype = ctypes.c_long
        for n in ('U_LZW_load', 'U_LOAD_near', 'U_LOAD_far'):
            getattr(lib, n).restype = ctypes.c_void_p
            getattr(lib, n).argtypes = [ctypes.c_char_p]
        lib.dos_farfree.argtypes = [ctypes.c_void_p]
        self.palette = (ctypes.c_uint8 * 768).in_dll(lib, 'vga_palette')
        self.waits = []
        self.wait_snap = None
        self._cb = WAITCB(self._wait)
        lib.shim_set_wait_cb(self._cb)

    def _wait(self, kind, arg):
        self.waits.append(kind if self.wait_snap is None else (kind, self.wait_snap()))

    def ptr(self, lin):
        return self.arena + lin

    def lin(self, p):
        return self.lib.shim_linear(p)

    def read(self, lin, n):
        return ctypes.string_at(self.arena + lin, n)

    def write(self, lin, data):
        ctypes.memmove(self.arena + lin, bytes(data), len(data))

    def var16(self, name):
        return ctypes.c_int16.in_dll(self.lib, name)

    def varp(self, name):
        return ctypes.c_void_p.in_dll(self.lib, name)


# ---------------------------------------------------------------- the original

class Machine:
    def __init__(self, image, syms, drv):
        self.syms = syms
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, 0x120000)
        uc.mem_write(LOAD * 16, bytes(image))
        uc.mem_write(DRVSEG * 16, bytes(drv))
        uc.mem_write(STOP, b'\xf4')
        uc.mem_write(STUBSEG * 16, b'\xcb' * 16)   # RETF stubs
        uc.mem_write(0x449, b'\x13')               # BIOS video mode
        uc.mem_write(0x463, struct.pack('<H', 0x3D4))
        self.ds = LOAD + DGROUP
        self.ints = {}
        self.port_in = {}
        self.dac = bytearray(768)
        self.dac_w = self.dac_r = 0
        self.retrace = 0
        self.retrace_fixed = None
        self.traps = {}
        self.heap_near = NEAR_HEAP[0]
        self.far_queue = []        # what farmalloc/farcalloc hand out: linear addresses
        self.far_log = []          # ('farmalloc', size) / ('farcalloc', n, size) / ('farfree', lin)
        self.near_log = []
        self.keys = []
        self.files = {}
        self.next_handle = 5
        self.mouse = {'present': 1, 'x': 0, 'y': 0, 'b': 0}
        self.ticks = 0
        uc.hook_add(UC_HOOK_INTR, self._intr)
        uc.hook_add(UC_HOOK_INSN, self._in, None, 1, 0, UC_X86_INS_IN)
        uc.hook_add(UC_HOOK_INSN, self._out, None, 1, 0, UC_X86_INS_OUT)
        for name in ('malloc', 'free', 'calloc', 'farmalloc', 'farfree', 'farcalloc', 'getch', 'kbhit'):
            cs, ip = self.entry(name)
            uc.mem_write(cs * 16 + ip, bytes([0xCD, TRAP, 0xCB]))
            self.traps[(cs, ip + 2)] = name
        # the driver table of MOUSECUR.ASM, and gfxDrv
        for i, e in enumerate((3, 6, 9, 0x12, 0x0C)):
            self.dsw(D_DRVTABLE + 4 * i, e)
            self.dsw(D_DRVTABLE + 4 * i + 2, DRVSEG)
        self.dsw(D_GFXDRV, 0)
        self.dsw(D_GFXDRV + 2, DRVSEG)
        self.wait_hooks = {}
        self.wait_log = []
        self.wait_snap = None
        for name in ('VID_waitRetrace', 'VID_inRetrace', 'waitTick'):
            cs, ip = self.entry(name)
            a = cs * 16 + ip
            self.wait_hooks[a] = name
            uc.hook_add(UC_HOOK_CODE, self._waithook, None, a, a)

    def entry(self, name):
        f, o = self.syms[name]
        return LOAD + f, o

    def _waithook(self, uc, addr, size, ud):
        name = self.wait_hooks[addr]
        kind = {'VID_waitRetrace': 1, 'VID_inRetrace': 2, 'waitTick': 3}[name]
        self.wait_log.append(kind if self.wait_snap is None else (kind, self.wait_snap()))

    # -- registers and memory
    def r(self, reg):
        return self.uc.reg_read(reg)

    def setcf(self, on):
        f = self.uc.reg_read(UC_X86_REG_EFLAGS)
        self.uc.reg_write(UC_X86_REG_EFLAGS, (f | 1) if on else (f & ~1))

    def rd16(self, lin):
        return struct.unpack('<H', self.uc.mem_read(lin, 2))[0]

    def wr16(self, lin, v):
        self.uc.mem_write(lin, struct.pack('<H', w16(v)))

    def dsw(self, off, v=None):
        if v is None:
            return self.rd16(self.ds * 16 + off)
        self.wr16(self.ds * 16 + off, v)

    def read(self, lin, n):
        return bytes(self.uc.mem_read(lin, n))

    def write(self, lin, data):
        self.uc.mem_write(lin, bytes(data))

    def dsread(self, off, n):
        return self.read(self.ds * 16 + off, n)

    def dswrite(self, off, data):
        self.write(self.ds * 16 + off, data)

    def cstr(self, lin):
        s = self.read(lin, 80)
        return s.split(b'\0')[0].decode('latin-1')

    # -- the machine's devices
    def _in(self, uc, port, size, ud):
        if port == 0x3DA:
            if self.retrace_fixed is not None:
                return self.retrace_fixed
            self.retrace ^= 8
            return self.retrace ^ 8
        if port == 0x3C9:
            v = self.dac[self.dac_r % 768]
            self.dac_r = (self.dac_r + 1) % 768
            return v
        f = self.port_in.get(port)
        return f() if f else 0xFF

    def _out(self, uc, port, size, value, ud):
        if port == 0x3C8:
            self.dac_w = (value & 0xFF) * 3
        elif port == 0x3C7:
            self.dac_r = (value & 0xFF) * 3
        elif port == 0x3C9:
            self.dac[self.dac_w % 768] = value & 0x3F
            self.dac_w = (self.dac_w + 1) % 768

    def _intr(self, uc, intno, ud):
        if intno == TRAP:
            self._trap()
            return
        if intno == 0x21:
            self._int21()
            return
        if intno == 0x33:
            ax = self.r(UC_X86_REG_AX)
            if ax == 0:
                uc.reg_write(UC_X86_REG_AX, 0xFFFF if self.mouse['present'] else 0)
                uc.reg_write(UC_X86_REG_BX, 2)
            elif ax == 3:
                uc.reg_write(UC_X86_REG_CX, self.mouse['x'] * 2)
                uc.reg_write(UC_X86_REG_DX, self.mouse['y'])
                uc.reg_write(UC_X86_REG_BX, self.mouse['b'])
            else:
                raise RuntimeError('int 33h AX=%X' % ax)
            return
        if intno == 0x1A:
            self.ticks += 1
            uc.reg_write(UC_X86_REG_DX, self.ticks & 0xFFFF)
            uc.reg_write(UC_X86_REG_CX, self.ticks >> 16)
            return
        raise RuntimeError('unexpected int %02Xh AX=%04X at %04X:%04X' % (
            intno, self.r(UC_X86_REG_AX), self.r(UC_X86_REG_CS), self.r(UC_X86_REG_IP)))

    def _arg(self, i):
        sp = self.r(UC_X86_REG_SS) * 16 + self.r(UC_X86_REG_SP)
        return self.rd16(sp + 4 + 2 * i)

    def _trap(self):
        name = self.traps[(self.r(UC_X86_REG_CS), self.r(UC_X86_REG_IP))]
        uc = self.uc
        if name in ('malloc', 'calloc'):
            n = self._arg(0) if name == 'malloc' else w16(self._arg(0) * self._arg(1))
            p = self.heap_near
            self.heap_near = (self.heap_near + n + 15) & ~15
            if self.heap_near > NEAR_HEAP[1]:
                self.heap_near = NEAR_HEAP[0]
                p = self.heap_near
                self.heap_near = (p + n + 15) & ~15
            if name == 'calloc':
                self.dswrite(p, bytes(n))
            self.near_log.append((name, n))
            uc.reg_write(UC_X86_REG_AX, p if n else 0)
        elif name == 'free':
            self.near_log.append(('free', self._arg(0)))
        elif name in ('farmalloc', 'farcalloc'):
            if name == 'farmalloc':
                size = self._arg(0) | (self._arg(1) << 16)
                self.far_log.append(('farmalloc', size))
            else:
                n = self._arg(0) | (self._arg(1) << 16)
                size = self._arg(2) | (self._arg(3) << 16)
                self.far_log.append(('farcalloc', n, size))
            lin = self.far_queue.pop(0) if self.far_queue else 0
            if lin and name == 'farcalloc':
                self.write(lin, bytes((n * size) & 0xFFFFF))
            uc.reg_write(UC_X86_REG_AX, lin & 15 if lin else 0)
            uc.reg_write(UC_X86_REG_DX, lin >> 4)
        elif name == 'farfree':
            self.far_log.append(('farfree', self._arg(1) * 16 + self._arg(0)))
        elif name == 'getch':
            uc.reg_write(UC_X86_REG_AX, self.keys.pop(0) if self.keys else 0x0D)
        elif name == 'kbhit':
            uc.reg_write(UC_X86_REG_AX, 0xFFFF if self.keys else 0)

    def _int21(self):
        uc = self.uc
        ax = self.r(UC_X86_REG_AX)
        ah = ax >> 8
        self.setcf(False)
        if ah == 0x3D:
            name = self.cstr(self.r(UC_X86_REG_DS) * 16 + self.r(UC_X86_REG_DX))
            p = gamefile(name)
            if not p:
                uc.reg_write(UC_X86_REG_AX, 2)
                self.setcf(True)
                return
            h = self.next_handle
            self.next_handle += 1
            self.files[h] = [open(p, 'rb').read(), 0]
            uc.reg_write(UC_X86_REG_AX, h)
        elif ah == 0x3E:
            if self.files.pop(self.r(UC_X86_REG_BX), None) is None:
                uc.reg_write(UC_X86_REG_AX, 6)
                self.setcf(True)
        elif ah == 0x3F:
            f = self.files.get(self.r(UC_X86_REG_BX))
            if f is None:
                uc.reg_write(UC_X86_REG_AX, 6)
                self.setcf(True)
                return
            n = self.r(UC_X86_REG_CX)
            data = f[0][f[1]:f[1] + n]
            f[1] += len(data)
            self.write(self.r(UC_X86_REG_DS) * 16 + self.r(UC_X86_REG_DX), data)
            uc.reg_write(UC_X86_REG_AX, len(data))
        elif ah == 0x42:
            f = self.files[self.r(UC_X86_REG_BX)]
            off = (self.r(UC_X86_REG_CX) << 16) | self.r(UC_X86_REG_DX)
            base = [0, f[1], len(f[0])][ax & 0xFF]
            f[1] = (base + off) & 0xFFFFFFFF
            uc.reg_write(UC_X86_REG_AX, f[1] & 0xFFFF)
            uc.reg_write(UC_X86_REG_DX, f[1] >> 16)
        else:
            raise RuntimeError('int 21h AH=%02X' % ah)

    def call(self, name, args=(), max_insns=400000000):
        """a far call of the routine with the arguments (int: a word; ('far', seg, off);
        ('long', v)), cdecl; returns (ax, dx)"""
        cs, ip = self.entry(name)
        words = []
        for a in args:
            if isinstance(a, tuple):
                if a[0] == 'far':
                    words += [w16(a[2]), w16(a[1])]
                else:
                    words += [w16(a[1]), w16(a[1] >> 16)]
            else:
                words.append(w16(a))
        uc = self.uc
        sp = 0xFFF0 - 2 * len(words) - 4
        ss = self.ds   # Turbo C's near data: SS = DGROUP (va_arg and &local are DS offsets)
        self.write(ss * 16 + sp, struct.pack('<HH', STOP & 0xF, STOP >> 4))
        if words:
            self.write(ss * 16 + sp + 4, struct.pack('<%dH' % len(words), *words))
        uc.reg_write(UC_X86_REG_SS, ss)
        uc.reg_write(UC_X86_REG_SP, sp)
        uc.reg_write(UC_X86_REG_DS, self.ds)
        uc.reg_write(UC_X86_REG_ES, self.ds)
        uc.reg_write(UC_X86_REG_CS, cs)
        uc.reg_write(UC_X86_REG_EFLAGS, 0x0202)
        uc.emu_start(cs * 16 + ip, STOP, 0, max_insns)
        if uc.reg_read(UC_X86_REG_CS) * 16 + uc.reg_read(UC_X86_REG_IP) != STOP:
            raise RuntimeError('%s did not return (instruction limit)' % name)
        return self.r(UC_X86_REG_AX), self.r(UC_X86_REG_DX)

    # -- U's structures in DGROUP
    def put_surface(self, off, seg, scan_seg, scan_off, right, left, top, bottom):
        self.dswrite(off, struct.pack('<HHHhhhh', seg, scan_off, scan_seg, right, left, top, bottom))

    def get_surface(self, off):
        return struct.unpack('<HHHhhhh', self.dsread(off, 14))

    def put_text(self, off, surf, x, y, font_seg, font_off, fg, bg, buf, bufptr, justify):
        self.dswrite(off, struct.pack('<HhhHHBBHHB', surf, x, y, font_off, font_seg, fg, bg, buf,
                                      bufptr, justify))

    def get_text(self, off):
        return struct.unpack('<HhhHHBBHHB', self.dsread(off, 17))


class Report:
    def __init__(self):
        self.failed = 0

    def result(self, name, ok, detail):
        print('%-6s %s  %s' % (name, 'PASS' if ok else 'FAIL', detail), flush=True)
        if not ok:
            self.failed += 1


def first_diff(a, b):
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return i
    return None if len(a) == len(b) else min(len(a), len(b))


def screen_table():
    return [y * 320 for y in range(200)]


class World:
    """the shared memory of both sides: screen, a far surface, font, shapes"""

    def __init__(self, m, c, rnd):
        self.m, self.c, self.rnd = m, c, rnd
        self.font = unz('U6.SET')
        self.shapes = unz('INTRO.PTR')
        for lin, data in ((FONT_LIN, self.font), (SHAPES_LIN, self.shapes)):
            m.write(lin, data)
            c.write(lin, data)
        # the screen's table in DGROUP (asm) and in host memory (C)
        tab = struct.pack('<200H', *screen_table())
        m.dswrite(DS_SCAN, tab)
        self.c_scan = ctypes.create_string_buffer(tab, 400)
        # the far surface's table at FARSCAN_LIN, both sides
        m.write(FARSCAN_LIN, tab)
        c.write(FARSCAN_LIN, tab)

    def noise(self, lin, n, colors=None):
        r = self.rnd
        if colors is None:
            data = bytes(r.getrandbits(8) for _ in range(n)) if n < 4096 else r.randbytes(n)
        else:
            data = bytes(r.choice(colors) for _ in range(n))
        self.m.write(lin, data)
        self.c.write(lin, data)

    def c_surface(self, far, right, left, top, bottom):
        if far:
            return CSurface(FARPIX_SEG, self.c.ptr(FARSCAN_LIN), right, left, top, bottom)
        return CSurface(SCREEN_SEG, ctypes.addressof(self.c_scan), right, left, top, bottom)

    def m_surface(self, off, far, right, left, top, bottom):
        if far:
            self.m.put_surface(off, FARPIX_SEG, FARSCAN_LIN >> 4, FARSCAN_LIN & 15, right, left, top, bottom)
        else:
            self.m.put_surface(off, SCREEN_SEG, self.m.ds, DS_SCAN, right, left, top, bottom)

    def same(self, lin, n):
        a, b = self.m.read(lin, n), self.c.read(lin, n)
        return first_diff(a, b)


# ---------------------------------------------------------------- TEXT

def rand_text(rnd, n):
    out = []
    while len(''.join(out)) < n:
        w = ''.join(rnd.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,;:!?\'-')
                    for _ in range(rnd.randrange(1, 12)))
        out.append(w)
        k = rnd.random()
        if k < 0.75:
            out.append(' ')
        elif k < 0.85:
            out.append('  ')
        elif k < 0.95:
            out.append('\n')
        else:
            out.append('\r')
    s = ''.join(out)
    if rnd.random() < 0.5:
        s = s.rstrip(' \n\r')
    return s[:250]


def rand_format(rnd, near_strings):
    """(format, [(kind, value)]): kinds d u D U x c s X Y F B"""
    parts, args = [], []
    for _ in range(rnd.randrange(1, 6)):
        if rnd.random() < 0.4:
            parts.append(rand_text(rnd, rnd.randrange(1, 20)).replace('%', ''))
            continue
        k = rnd.choice('duDUxcsXYFBq')
        if k == 'q':
            parts.append('%' + rnd.choice('%zq!'))   # another character after %
            continue
        parts.append('%' + k)
        if k in 'dux':
            v = rnd.choice([0, 1, -1, 7, 99, 32767, -32768, 65535, rnd.randrange(-40000, 70000)])
        elif k in 'DU':
            v = rnd.choice([0, -1, 123456789, -2147483648, 4294967295, rnd.randrange(-3000000000, 5000000000)])
        elif k == 'c':
            v = rnd.randrange(32, 127)
        elif k == 's':
            v = rnd.randrange(len(near_strings))
        elif k in 'XY':
            v = None   # chosen inside the window
        else:
            v = rnd.randrange(256)
        args.append((k, v))
    return ''.join(parts), args


def test_text(m, c, rep, rnd):
    world = World(m, c, rnd)
    lib = c.lib
    c_buf = ctypes.create_string_buffer(0x400)
    c_str = ctypes.create_string_buffer(1024)
    near_strings = ['', 'Avatar', 'a b c', 'Lord British', 'x' * 30, 'Britannia!']
    # the %s strings: in DGROUP for the original (near pointers), host strings for the C
    s_off, off = [], DS_STR + 0x300
    c_strings = []
    for s in near_strings:
        m.dswrite(off, s.encode() + b'\0')
        s_off.append(off)
        c_strings.append(ctypes.create_string_buffer(s.encode()))
        off += len(s) + 1
    c_font = c.ptr(FONT_LIN)
    rounds = 12 if QUICK else 60
    nops = 0
    bad = None
    for rnd_i in range(rounds):
        far = rnd_i % 3 == 2
        world.noise((FARPIX_SEG if far else SCREEN_SEG) * 16, 64000)
        left = rnd.randrange(0, 200)
        right = rnd.randrange(left + 24, 320)
        top = rnd.randrange(0, 150)
        bottom = rnd.randrange(top + 10, 200)
        fg, bg = rnd.randrange(256), rnd.randrange(256)
        if rnd.random() < 0.2:
            fg, bg = world.font[2], world.font[3]
        just = rnd.choice([0, 1, 2, 3, 3, 4])
        world.m_surface(DS_SURF, far, right, left, top, bottom)
        csurf = world.c_surface(far, right, left, top, bottom)
        m.put_text(DS_TEXT, DS_SURF, left, top, FONT_LIN >> 4, FONT_LIN & 15, fg, bg, DS_BUF, DS_BUF, just)
        ct = CText(ctypes.addressof(csurf), left, top, c_font, fg, bg, ctypes.addressof(c_buf),
                   ctypes.addressof(c_buf), just)
        m.dswrite(DS_BUF, bytes(0x400))
        ctypes.memset(c_buf, 0, 0x400)
        m.call('TXT_select', [DS_TEXT])
        lib.U_TXT_select(ctypes.byref(ct))
        for _ in range(40):
            k = rnd.random()
            if k < 0.3:
                s = rand_text(rnd, rnd.randrange(1, 250))
                if rnd.random() < 0.3:
                    # (no space after a 10: a fully justified line that starts with spaces
                    # prints its first word up to a space, past the string's end if need be)
                    s = rand_text(rnd, 8).rstrip(' \n\r') + ' ' * rnd.randrange(1, 4) + rand_text(rnd, 60)
                op = ('puts', s)
                m.dswrite(DS_STR, s.encode() + b'\0')
                c_str.value = s.encode()
                m.call('TXT_puts', [('far', m.ds, DS_STR)])
                lib.U_TXT_puts(c_str)
            elif k < 0.4:
                ch = rnd.choice([rnd.randrange(32, 127), 10, 13, rnd.randrange(128, 256)])
                op = ('putc', ch)
                m.call('TXT_putc', [ch])
                lib.U_TXT_putc(ch)
            elif k < 0.7:
                fmt, args = rand_format(rnd, near_strings)
                fn = rnd.choice(['TXT_printf', 'TXT_sprintf', 'TXT_sappend'])
                if fn == 'TXT_printf' and rnd.random() < 0.1:
                    fmt += '%P'
                m_args, c_args = [], []
                for kind, v in args:
                    if kind in 'XY':
                        v = rnd.randrange(left, right - 10) if kind == 'X' else rnd.randrange(top, bottom - 8)
                    if kind == 's':
                        m_args.append(s_off[v])
                        c_args.append(c_strings[v])
                    elif kind in 'DU':
                        m_args.append(('long', v & 0xFFFFFFFF))
                        v32 = v & 0xFFFFFFFF
                        c_args.append(ctypes.c_int32(v32 - (1 << 32) if v32 & 0x80000000 else v32))
                    else:
                        m_args.append(w16(v))
                        c_args.append(ctypes.c_int(s16(v)))
                op = (fn, fmt, args)
                m.dswrite(DS_STR, fmt.encode() + b'\0')
                c_fmt = ctypes.create_string_buffer(fmt.encode())
                m.call(fn, [('far', m.ds, DS_STR)] + m_args)
                getattr(lib, 'U_' + fn)(c_fmt, *c_args)
            elif k < 0.76:
                op = ('clear',)
                m.call('TXT_clear')
                lib.U_TXT_clear()
            elif k < 0.8:
                op = ('clearEos',)
                m.call('TXT_clearEos')
                lib.U_TXT_clearEos()
            elif k < 0.86:
                op = ('clreol',)
                m.call('clreol')
                lib.U_clreol()
            elif k < 0.93:
                x, y = rnd.randrange(left, right - 10), rnd.randrange(top, bottom - 4)
                op = ('gotoxy', x, y)
                m.call('gotoxy', [x, y])
                lib.U_gotoxy(x, y)
            else:
                n = rnd.randrange(1, 3)
                fn = rnd.choice(['TXT_scrollUp', 'TXT_scrollDown'])
                op = (fn, n)
                m.call(fn, [n])
                getattr(lib, 'U_' + fn)(n)
            nops += 1
            seg = FARPIX_SEG if far else SCREEN_SEG
            d = world.same(seg * 16, 64000)
            mt = m.get_text(DS_TEXT)
            cfields = (ct.curX, ct.curY, ct.fgColor, ct.bgColor, ct.bufPtr - ctypes.addressof(c_buf), ct.justify)
            mfields = (mt[1], mt[2], mt[5], mt[6], mt[8] - DS_BUF, mt[9])
            mb, cb = m.dsread(DS_BUF, 0x400), c_buf.raw
            if d is not None or cfields != mfields or mb != cb:
                bad = 'round %d (window %d..%d x %d..%d, %s, justify %d) op %r: pixel %s, text asm %s C %s, buffer %s' % (
                    rnd_i, left, right, top, bottom, 'far' if far else 'screen', just, op,
                    None if d is None else '%d (%d,%d)' % (d, d % 320, d // 320), mfields, cfields,
                    'same' if mb == cb else 'differs at %d' % first_diff(mb, cb))
                break
        if bad:
            break
    rep.result('TEXT', bad is None, '%d operations in %d windows%s' % (nops, rounds, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- SURF

def test_surf(m, c, rep, rnd):
    lib = c.lib
    n = 0
    bad = None
    for i in range(40 if QUICK else 200):
        w = rnd.choice([1, 2, 3, 15, 16, 17, 100, 319, 320, rnd.randrange(1, 321)])
        h = rnd.choice([1, 2, 7, 199, 200, rnd.randrange(1, 201)])
        left = rnd.choice([0, 0, rnd.randrange(0, 50)])
        cs = CSurface(0, None, left + w - 1, left, 0, h - 1)
        lib.U_SURF_alloc(ctypes.byref(cs))
        tab_lin = c.lin(cs.pScanlines)
        tab = c.read(tab_lin, 2 * h)
        pix_lin = cs.segment * 16 + struct.unpack_from('<H', tab)[0]
        m.far_queue[:] = [pix_lin, tab_lin]
        m.far_log[:] = []
        m.put_surface(DS_SURF, 0, 0, 0, left + w - 1, left, 0, h - 1)
        m.call('SURF_alloc', [DS_SURF])
        seg, soff, sseg = m.get_surface(DS_SURF)[:3]
        mtab = m.read(sseg * 16 + soff, 2 * h)
        c_hdr = [struct.unpack('<I', c.read(lin - 8, 4))[0] for lin in (pix_lin, tab_lin)]
        want = [('farcalloc', 1, w * h), ('farmalloc', 2 * h)]
        sizes = [w * h, 2 * h]
        ok = (seg == cs.segment and sseg * 16 + soff == tab_lin and mtab == tab and m.far_log == want
              and all(c_hdr[k] == ((sizes[k] + 8 + 15) & ~15) | 1 for k in range(2)))
        n += 1
        if not ok:
            bad = 'SURF_alloc %dx%d: asm seg %04X table %05X %s, C seg %04X table %05X; tables %s; heap %s' % (
                w, h, seg, sseg * 16 + soff, m.far_log, cs.segment, tab_lin, mtab == tab, [hex(x) for x in c_hdr])
            break
        # SURF_free: what is freed
        m.far_log[:] = []
        m.call('SURF_free', [DS_SURF])
        lib.U_SURF_free(ctypes.byref(cs))
        c_hdr = [struct.unpack('<I', c.read(lin - 8, 4))[0] for lin in (pix_lin, tab_lin)]
        if m.far_log != [('farfree', tab_lin), ('farfree', pix_lin)] or any(h_ & 1 for h_ in c_hdr):
            bad = 'SURF_free %dx%d: asm frees %s, C headers %s' % (w, h, m.far_log, [hex(x) for x in c_hdr])
            break
    # SURF_share
    if bad is None:
        for i in range(100):
            k = rnd.randrange(1, 5)
            vals = [(rnd.randrange(0x10000), rnd.randrange(0x10000), rnd.randrange(0x10000),
                     rnd.randrange(-500, 500), rnd.randrange(-500, 500), rnd.randrange(-500, 500),
                     rnd.randrange(-500, 500)) for _ in range(k + 1)]
            cs = []
            for j, v in enumerate(vals):
                m.put_surface(DS_SURF + 16 * j, v[0], v[2], v[1], *v[3:])
                cs.append(CSurface(v[0], c.ptr(0x50000 + v[1]), *v[3:]))
            m.call('SURF_share', [DS_SURF + 16 * j for j in range(k + 1)] + [0])
            lib.U_SURF_share(*([ctypes.byref(s) for s in cs] + [ctypes.c_void_p(0)]))
            for j in range(k + 1):
                a = m.get_surface(DS_SURF + 16 * j)
                b = (cs[j].segment, c.lin(cs[j].pScanlines) - 0x50000, cs[j].right, cs[j].left, cs[j].top, cs[j].bottom)
                if (a[0], a[1]) + a[3:] != b or (a[2] != vals[0][2] if j else False):
                    bad = 'SURF_share of %d: surface %d asm %s C %s' % (k, j, a, b)
            n += 1
            if bad:
                break
    rep.result('SURF', bad is None, '%d SURF_alloc/free/share compared%s' % (n, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- DRAW

def test_draw(m, c, rep, rnd):
    world = World(m, c, rnd)
    lib = c.lib
    n = 0
    bad = None
    for r in range(20 if QUICK else 80):
        world.noise(SCREEN_SEG * 16, 64000)
        if r % 4 == 0:
            right, left, top, bottom = 319, 0, 0, 199
        else:
            # inside the screen: a row outside it would read past the scanline table
            left, top = rnd.randrange(0, 200), rnd.randrange(0, 150)
            right, bottom = rnd.randrange(left, 320), rnd.randrange(top, 200)
        world.m_surface(DS_SURF, False, right, left, top, bottom)
        cs = world.c_surface(False, right, left, top, bottom)
        for _ in range(40):
            def coord(lo, hi):
                k = rnd.random()
                if k < 0.8:
                    return rnd.randrange(lo, hi)
                if k < 0.9:
                    return rnd.choice([-32768, 32767, -1, 0, 30000, -30000])
                return rnd.randrange(-32768, 32768)
            x1, y1, x2, y2 = coord(-60, 380), coord(-60, 260), coord(-60, 380), coord(-60, 260)
            if rnd.random() < 0.2:
                y2 = y1
            col = rnd.randrange(256)
            m.call('DRW_line', [DS_SURF, x1, y1, x2, y2, col])
            lib.U_DRW_line(ctypes.byref(cs), x1, y1, x2, y2, col)
            n += 1
            d = world.same(SCREEN_SEG * 16, 64000)
            if d is not None:
                bad = 'DRW_line(%d,%d, %d,%d) in %d..%d x %d..%d: pixel (%d,%d)' % (
                    x1, y1, x2, y2, left, right, top, bottom, d % 320, d // 320)
                break
            x, y = rnd.randrange(-10, 330), rnd.randrange(-10, 210)
            a, _ = m.call('DRW_getPixel', [DS_SURF, x, y])
            b = lib.U_DRW_getPixel(ctypes.byref(cs), x, y)
            n += 1
            if s16(a) != b:
                bad = 'DRW_getPixel(%d,%d): asm %d C %d' % (x, y, s16(a), b)
                break
        if bad:
            break
    rep.result('DRAW', bad is None, '%d DRW_line/DRW_getPixel compared%s' % (n, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- MOUSE

def test_mouse(m, c, rep, rnd):
    world = World(m, c, rnd)
    lib = c.lib
    shapes = world.shapes
    nshapes = (struct.unpack_from('<I', shapes, 4)[0] - 4) // 4
    world.noise(SCREEN_SEG * 16, 64000)
    world.noise(SAVE_LIN, 0x400)
    # onMove: a RETF stub counted on the original's side, a ctypes callback on the C's
    moves = {'m': 0, 'c': 0}

    def onmove_hook(uc, addr, size, ud):
        moves['m'] += 1
    m.uc.hook_add(UC_HOOK_CODE, onmove_hook, None, STUBSEG * 16, STUBSEG * 16)
    ONMOVE = ctypes.CFUNCTYPE(None)

    def c_onmove():
        moves['c'] += 1
    c_onmove_cb = ONMOVE(c_onmove)
    world.m_surface(DS_SURF, False, 319, 0, 0, 199)
    csurf = world.c_surface(False, 319, 0, 0, 199)
    cm = CMouse()
    area = CSurface()
    sem = c.var16('U_nMouseSem')
    m.retrace_fixed = 8
    lib.U_MOUSE_POINTER.argtypes = [ctypes.c_void_p, ctypes.c_int16]
    lib.SHAPE_IN_RECT = lib.U_SHAPE_IN_RECT
    lib.U_SHAPE_IN_RECT.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int16, ctypes.c_int16, ctypes.c_int16]

    def init(no):
        mb = bytearray(25)
        struct.pack_into('<HH', mb, 8, SHAPES_LIN & 15, SHAPES_LIN >> 4)
        struct.pack_into('<H', mb, 0x0E, DS_SURF)
        struct.pack_into('<HH', mb, 0x10, SAVE_LIN & 15, SAVE_LIN >> 4)
        m.dswrite(DS_MOUSE, bytes(mb))
        ctypes.memset(ctypes.addressof(cm), 0, ctypes.sizeof(cm))
        cm.shapes = c.ptr(SHAPES_LIN)
        cm.surface = ctypes.addressof(csurf)
        cm.saveBuf = c.ptr(SAVE_LIN)
        a, _ = m.call('MOUSE_NOINIT' if no else 'MOUSE_INIT', [DS_MOUSE])
        b = (lib.U_MOUSE_NOINIT if no else lib.U_MOUSE_INIT)(ctypes.byref(cm))
        # main.c installs no onMove; the test's counts the calls
        m.wr16(m.ds * 16 + DS_MOUSE + 0x14, 0)
        m.wr16(m.ds * 16 + DS_MOUSE + 0x16, STUBSEG)
        cm.onMove = ctypes.cast(c_onmove_cb, ctypes.c_void_p).value
        return a, b

    def m_struct():
        b = m.dsread(DS_MOUSE, 25)
        return (b[0], b[1], s16(struct.unpack_from('<H', b, 2)[0]), s16(struct.unpack_from('<H', b, 4)[0]),
                s16(struct.unpack_from('<H', b, 0x0C)[0]), b[0x18])

    def c_struct():
        return (cm.buttonL, cm.buttonR, cm.x, cm.y, cm.shape, cm.installed)

    def rect(x0, y0, x1, y1):
        m.put_surface(DS_SURF + 0x20, 0, 0, 0, x1, x0, y0, y1)
        area.right, area.left, area.top, area.bottom = x1, x0, y0, y1

    bad = None
    nops = 0
    hits = 0
    rounds = 10 if QUICK else 50
    hdrs = []
    for i in range(nshapes):
        off = struct.unpack_from('<I', shapes, 4 + 4 * i)[0]
        hdrs.append(struct.unpack_from('<4h', shapes, off))

    def sweep():
        # MOUSE_IN_AREA's and SHAPE_IN_RECT's four compares, one pixel either side
        ops = [('INIT',), ('SHOW',)]
        for _ in range(40 if QUICK else 200):
            idx = rnd.randrange(nshapes)
            x, y = rnd.randrange(320), rnd.randrange(200)
            ops += [('SERVE', x, y, 0), ('POINTER', idx)]
            rt, lf, up, dn = hdrs[idx]
            for d in (-1, 0, 1):
                ops.append(('IN_AREA', x - lf + d - 50, y - 50, x - lf + d, y + 50, 0, 0, 0))
                ops.append(('IN_AREA', x + rt + d, y - 50, x + rt + d + 50, y + 50, 0, 0, 0))
                ops.append(('IN_AREA', x - 50, y - up + d - 50, x + 50, y - up + d, 0, 0, 0))
                ops.append(('IN_AREA', x - 50, y + dn + d, x + 50, y + dn + d + 50, 0, 0, 0))
                ops.append(('SHAPE', x - lf + d, y - up - 5, x + rt + 5, y + dn + 5, idx, x, y))
                ops.append(('SHAPE', x - lf - 5, y - up - 5, x + rt + d, y + dn + 5, idx, x, y))
                ops.append(('SHAPE', x - lf - 5, y - up + d, x + rt + 5, y + dn + 5, idx, x, y))
                ops.append(('SHAPE', x - lf - 5, y - up - 5, x + rt + 5, y + dn + d, idx, x, y))
                ops.append(('IN_RECT', x + d, y, x + 10, y + 10, 0, 0, 0))
                ops.append(('IN_RECT', x - 10, y - 10, x + d, y + d, 0, 0, 0))
        return ops
    for r in range(rounds + 1):
        m.dsw(D_MOUSESEM, 0)
        sem.value = 0
        present = 0 if r % 7 == 6 and r < rounds else 1
        m.mouse.update(present=present, x=rnd.randrange(320), y=rnd.randrange(200), b=0)
        lib.stub_mouse_set(present, m.mouse['x'], m.mouse['y'], 0)
        ops = sweep() if r == rounds else [('NOINIT' if r % 11 == 10 else 'INIT',)]
        gx, gy = m.mouse['x'], m.mouse['y']
        for _ in range(80 if r < rounds else 0):
            k = rnd.random()
            if k < 0.3:
                if rnd.random() < 0.6:
                    gx, gy = rnd.randrange(320), rnd.randrange(200)
                    ops.append(('SERVE', gx, gy, rnd.randrange(4) if rnd.random() < 0.3 else None))
                else:
                    ops.append(('SERVE', None, None, rnd.randrange(4) if rnd.random() < 0.3 else None))
            elif k < 0.42:
                ops.append(('SHOW',))
            elif k < 0.52:
                ops.append(('HIDE',))
            elif k < 0.6:
                ops.append(('POINTER', rnd.randrange(nshapes)))
            elif k < 0.64:
                ops.append((rnd.choice(['DRAW', 'SAVE_BG']),))
            else:
                if rnd.random() < 0.6:   # around the pointer, or the shape: near the edges
                    x0, y0 = gx - rnd.randrange(-3, 30), gy - rnd.randrange(-3, 30)
                else:
                    x0, y0 = rnd.randrange(-30, 340), rnd.randrange(-30, 220)
                x1, y1 = x0 + rnd.randrange(-5, 60), y0 + rnd.randrange(-5, 60)
                sx, sy = rnd.randrange(x0 - 10, x1 + 11), rnd.randrange(y0 - 10, y1 + 11)
                ops.append((rnd.choice(['IN_AREA', 'IN_RECT', 'SHAPE']), x0, y0, x1, y1,
                            rnd.randrange(nshapes), sx, sy))
        for op in ops:
            moves['m'] = moves['c'] = 0
            kind = op[0]
            if kind in ('INIT', 'NOINIT'):
                a, b = init(kind == 'NOINIT')
                if kind == 'NOINIT':
                    a = b   # (MOUSE_NOINIT leaves AX as it was: no result)
            elif kind == 'SERVE':
                if op[1] is not None:
                    m.mouse['x'], m.mouse['y'] = op[1], op[2]
                if op[3] is not None:
                    m.mouse['b'] = op[3]
                lib.stub_mouse_set(present, m.mouse['x'], m.mouse['y'], m.mouse['b'])
                a, _ = m.call('MOUSE_SERVE')
                b = lib.U_MOUSE_SERVE()
            elif kind == 'POINTER':
                m.call('MOUSE_POINTER', [('far', SHAPES_LIN >> 4, SHAPES_LIN & 15), op[1]])
                lib.U_MOUSE_POINTER(c.ptr(SHAPES_LIN), op[1])
                a = b = 0
            elif kind in ('IN_AREA', 'IN_RECT'):
                rect(*op[1:5])
                a, _ = m.call('MOUSE_' + kind, [DS_SURF + 0x20])
                b = getattr(lib, 'U_MOUSE_' + kind)(ctypes.byref(area))
                hits += a == 1
            elif kind == 'SHAPE':
                rect(*op[1:5])
                a, _ = m.call('SHAPE_IN_RECT', [DS_SURF + 0x20, ('far', SHAPES_LIN >> 4, SHAPES_LIN & 15)] + list(op[5:8]))
                b = lib.U_SHAPE_IN_RECT(ctypes.byref(area), c.ptr(SHAPES_LIN), *op[5:8])
                hits += a == 1
            else:
                m.call('MOUSE_' + kind)
                getattr(lib, 'U_MOUSE_' + kind)()
                a = b = 0
            nops += 1
            d = world.same(SCREEN_SEG * 16, 64000)
            ds = world.same(SAVE_LIN, 0x400)
            if (w16(a) != w16(b) or d is not None or ds is not None or m_struct() != c_struct()
                    or s16(m.dsw(D_MOUSESEM)) != sem.value or moves['m'] != moves['c']):
                bad = 'round %d op %s: result asm %d C %d, screen %s, save %s, mouse asm %s C %s, sem asm %d C %d, onMove asm %d C %d' % (
                    r, op, s16(a), b, d, ds, m_struct(), c_struct(), s16(m.dsw(D_MOUSESEM)), sem.value,
                    moves['m'], moves['c'])
                break
        if bad:
            break
    m.retrace_fixed = None
    rep.result('MOUSE', bad is None, '%d operations (%d hits)%s' % (nops, hits, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- FADE

def test_fade(m, c, rep, rnd):
    world = World(m, c, rnd)
    lib = c.lib
    bad = None
    n = 0
    # FADE_step
    for i in range(50 if QUICK else 300):
        k = rnd.randrange(1, 40)
        cur = [rnd.choice([rnd.randrange(64), rnd.randrange(-300, 300)]) for _ in range(k)]
        tgt = [rnd.choice([rnd.randrange(64), rnd.randrange(-300, 300)]) for _ in range(k)]
        m.dswrite(DS_VEC, struct.pack('<%dh' % k, *cur))
        m.dswrite(DS_VEC + 0x100, struct.pack('<%dh' % k, *tgt))
        ccur = (ctypes.c_int16 * k)(*cur)
        ctgt = (ctypes.c_int16 * k)(*tgt)
        steps = 0
        while True:
            a, _ = m.call('FADE_step', [DS_VEC, DS_VEC + 0x100, k])
            b = lib.U_FADE_step(ccur, ctgt, k)
            steps += 1
            mc = list(struct.unpack('<%dh' % k, m.dsread(DS_VEC, 2 * k)))
            if a != b or mc != list(ccur):
                bad = 'FADE_step %s -> %s, step %d: asm %d %s C %d %s' % (cur, tgt, steps, a, mc, b, list(ccur))
                break
            if a == 0 or steps > 1000:
                break
        n += 1
        if bad:
            break
    # FADE_in, FADE_out: the DAC at each vertical retrace
    snaps = []
    m.wait_snap = lambda: bytes(m.dac)
    c.wait_snap = lambda: bytes(c.palette)
    for i in range(6 if QUICK else 30):
        if bad:
            break
        pal = bytes(rnd.randrange(64) for _ in range(768))
        m.dac[:] = pal
        ctypes.memmove(c.palette, pal, 768)
        colors = rnd.sample(range(256), rnd.randrange(1, 40))
        world.noise(SCREEN_SEG * 16, 64000, colors)
        world.noise(FARPIX_SEG * 16, 64000, rnd.sample(range(256), rnd.randrange(1, 40)))
        left, top = rnd.randrange(0, 200), rnd.randrange(0, 150)
        right, bottom = rnd.randrange(left, 320), rnd.randrange(top, 200)
        m.wait_log[:] = []
        c.waits[:] = []
        if i % 2:
            col = rnd.randrange(256)
            world.m_surface(DS_SURF, False, right, left, top, bottom)
            cs = world.c_surface(False, right, left, top, bottom)
            op = 'FADE_out(%d..%d x %d..%d, %d)' % (left, right, top, bottom, col)
            m.call('FADE_out', [DS_SURF, col])
            lib.U_FADE_out(ctypes.byref(cs), col)
        else:
            world.m_surface(DS_SURF, True, right, left, top, bottom)
            src = world.c_surface(True, right, left, top, bottom)
            dl, dt = rnd.randrange(0, 320 - (right - left)), rnd.randrange(0, 200 - (bottom - top))
            world.m_surface(DS_SURF + 16, False, 319, dl, dt, 199)
            dst = world.c_surface(False, 319, dl, dt, 199)
            op = 'FADE_in(%d..%d x %d..%d to %d,%d)' % (left, right, top, bottom, dl, dt)
            m.call('FADE_in', [DS_SURF, DS_SURF + 16])
            lib.U_FADE_in(ctypes.byref(src), ctypes.byref(dst))
        n += 1
        d = world.same(SCREEN_SEG * 16, 64000)
        if m.wait_log != c.waits or bytes(m.dac) != bytes(c.palette) or d is not None:
            k = first_diff(m.wait_log, c.waits)
            bad = '%s: waits asm %d C %d (first different #%s), DAC %s, screen %s' % (
                op, len(m.wait_log), len(c.waits), k, 'same' if bytes(m.dac) == bytes(c.palette) else 'differs', d)
        snaps.append(len(m.wait_log))
    m.wait_snap = c.wait_snap = None
    rep.result('FADE', bad is None, '%d FADE_step/FADE_in/FADE_out, %d retrace snapshots of the DAC compared%s' % (
        n, sum(snaps), (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- DISSOLVE

def test_dissolve(m, c, rep, rnd):
    world = World(m, c, rnd)
    lib = c.lib
    bad = None
    n = 0
    waits = 0
    crc = lambda side: lambda: zlib.crc32(side.read(SCREEN_SEG * 16, 0x10000))  # noqa: E731
    m.wait_snap = crc(m)
    c.wait_snap = crc(c)
    for i in range(9 if QUICK else 40):
        kind = ['DIS_dissolve', 'DIS_dissolveCGA', 'DIS_dissolveTDY'][i % 3]
        world.noise(SCREEN_SEG * 16, 0x10000)
        world.noise(FARPIX_SEG * 16, 0x10000)
        while True:
            w = rnd.choice([1, 2, 3, 5, 64, 100, 255, 256, 257, 319, 320, rnd.randrange(1, 321)])
            h = rnd.choice([1, 2, 7, 8, 100, 199, 200, rnd.randrange(1, 201)])
            if w * h >= 2:
                break
        if kind != 'DIS_dissolve':
            w = min(w, 80)
        left = rnd.randrange(0, 321 - w)
        top = rnd.randrange(0, 201 - h) if rnd.random() < 0.85 else rnd.randrange(0, 40)
        dl = rnd.randrange(0, 321 - w)
        dt = rnd.randrange(0, 201 - h) if top + h <= 200 else rnd.randrange(0, 40)
        speed = rnd.choice([1, 2, 10, rnd.randrange(0, 40)]) if kind == 'DIS_dissolve' else \
            rnd.choice([0x46, 0x78, 0x32, rnd.randrange(1, 400)])
        world.m_surface(DS_SURF, True, left + w - 1, left, top, top + h - 1)
        src = world.c_surface(True, left + w - 1, left, top, top + h - 1)
        world.m_surface(DS_SURF + 16, False, 319, dl, dt, 199)
        dst = world.c_surface(False, 319, dl, dt, 199)
        m.wait_log[:] = []
        c.waits[:] = []
        m.call(kind, [DS_SURF, DS_SURF + 16, speed])
        getattr(lib, 'U_' + kind)(ctypes.byref(src), ctypes.byref(dst), speed)
        n += 1
        waits += len(m.wait_log)
        d = world.same(SCREEN_SEG * 16, 0x10000)
        if d is not None or m.wait_log != c.waits:
            bad = '%s %dx%d at %d,%d to %d,%d speed %d: memory %s, waits asm %d C %d (first different #%s)' % (
                kind, w, h, left, top, dl, dt, speed, d, len(m.wait_log), len(c.waits), first_diff(m.wait_log, c.waits))
            break
    m.wait_snap = c.wait_snap = None
    # DIS_farcpy
    if bad is None:
        for i in range(200):
            nb = rnd.choice([0, 1, 2, 3, rnd.randrange(0, 3000)])
            a, b = rnd.randrange(0x50000, 0x5F000), rnd.randrange(0x50000, 0x5F000)
            world.noise(0x50000, 0x10000)
            m.call('DIS_farcpy', [('far', a >> 4, a & 15), ('far', b >> 4, b & 15), nb])
            lib.U_DIS_farcpy(ctypes.c_void_p(c.ptr(a)), ctypes.c_void_p(c.ptr(b)), nb)
            n += 1
            d = world.same(0x50000, 0x10000)
            if d is not None:
                bad = 'DIS_farcpy(%05X, %05X, %d): byte %X' % (a, b, nb, d)
                break
    rep.result('DISS', bad is None, '%d dissolves and copies, %d waits with the screen compared%s' % (
        n, waits, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- LZW, LOAD

def lzw_files():
    out = []
    for name in sorted(os.listdir(GAMEDATA)):
        p = os.path.join(GAMEDATA, name)
        if not os.path.isfile(p) or os.path.getsize(p) < 8:
            continue
        d = open(p, 'rb').read(8)
        size = struct.unpack_from('<I', d)[0]
        if (d[4] | (d[5] << 8)) & 0x1FF == 256 and 0 < size < 0x40000:
            out.append(name)
    return out


def test_lzw(m, c, rep, rnd):
    lib = c.lib
    files = lzw_files() + ['NOFILE.XYZ']
    if QUICK:
        files = files[::4] + ['NOFILE.XYZ']
    bad = None
    total = 0
    for name in files:
        p = lib.U_LZW_load(name.encode())
        m.far_log[:] = []
        m.near_log[:] = []
        m.dswrite(DS_STR, name.encode() + b'\0')
        if p:
            lin = c.lin(p)
            size = struct.unpack('<I', open(gamefile(name), 'rb').read(4))[0]
            m.far_queue[:] = [lin]
            m.write(lin, bytes(size))
            ax, dx = m.call('LZW_load', [DS_STR])
            got = m.read(lin, size)
            want = c.read(lin, size)
            hdr = struct.unpack('<I', c.read(lin - 8, 4))[0]
            asked = m.far_log[0][1] if m.far_log else None
            ok = (dx * 16 + ax == lin and got == want and asked == ((size >> 4) + 1) << 4
                  and hdr == ((asked + 8 + 15) & ~15) | 1 and want == unz(name)[:size])
            total += size
            lib.dos_farfree(p)
            if not ok:
                bad = '%s: asm %04X:%04X C %05X, asked %s, data %s' % (name, dx, ax, lin, asked, first_diff(got, want))
                break
        else:
            m.far_queue[:] = []
            ax, dx = m.call('LZW_load', [DS_STR])
            if ax or dx or gamefile(name):
                bad = '%s: C gives NULL, asm %04X:%04X' % (name, dx, ax)
                break
        if m.files:
            bad = '%s: the original left %d file(s) open' % (name, len(m.files))
            break
    rep.result('LZW', bad is None, '%d files, %d bytes unpacked%s' % (len(files), total, (': ' + bad) if bad else ''))


def test_load(m, c, rep, rnd):
    lib = c.lib
    libc = ctypes.CDLL(None)
    libc.free.argtypes = [ctypes.c_void_p]
    names = sorted(os.listdir(GAMEDATA))
    files = [n for n in names if os.path.isfile(os.path.join(GAMEDATA, n))
             and re.fullmatch(r'[A-Za-z0-9_$~!#-]{1,8}(\.[A-Za-z0-9_$~!#-]{1,3})?', n)]
    rnd.shuffle(files)
    small = [n for n in files if os.path.getsize(os.path.join(GAMEDATA, n)) < 0x6000][:30]   # the near heap: 7000h bytes
    big = [n for n in files if os.path.getsize(os.path.join(GAMEDATA, n)) >= 0x6000
           and os.path.getsize(os.path.join(GAMEDATA, n)) < 0x40000][:10]
    bad = None
    n = 0
    for name in small + big + ['NOFILE.XYZ']:
        fsize = os.path.getsize(gamefile(name)) if gamefile(name) else 0
        m.dswrite(DS_STR, name.encode() + b'\0')
        # LOAD_near (the small files: the emulated near heap has 7000h bytes)
        p = lib.U_LOAD_near(name.encode()) if fsize < 0x6000 else None
        m.near_log[:] = []
        ax = m.call('LOAD_near', [DS_STR])[0] if fsize < 0x6000 else 0
        if bool(p) != bool(ax):
            bad = 'LOAD_near(%s) (%d bytes): asm %04X C %s' % (name, fsize, ax, p)
            break
        if p:
            if m.dsread(ax, fsize) != ctypes.string_at(p, fsize) or m.near_log[0] != ('malloc', fsize):
                bad = 'LOAD_near(%s): contents or malloc %s' % (name, m.near_log)
                break
            libc.free(p)
        n += 1
        # LOAD_far
        p = lib.U_LOAD_far(name.encode())
        m.far_log[:] = []
        if p:
            lin = c.lin(p)
            m.far_queue[:] = [lin]
            m.write(lin, c.read(lin, fsize))   # the same memory before (a short read leaves it)
            m.write(lin, bytes(min(fsize, 0x10000)))
        else:
            m.far_queue[:] = []
        ax, dx = m.call('LOAD_far', [DS_STR])
        if bool(p) != bool(ax | dx) or (p and dx * 16 + ax != c.lin(p)):
            bad = 'LOAD_far(%s): asm %04X:%04X C %s' % (name, dx, ax, p)
            break
        if p:
            k = fsize & 0xFFFF
            hdr = struct.unpack('<I', c.read(lin - 8, 4))[0]
            if (m.read(lin, k) != c.read(lin, k) or m.far_log != [('farmalloc', fsize)]
                    or hdr != ((fsize + 8 + 15) & ~15) | 1):
                bad = 'LOAD_far(%s): contents or farmalloc %s' % (name, m.far_log)
                break
            lib.dos_farfree(p)
        n += 1
    rep.result('LOAD', bad is None, '%d LOAD_near/LOAD_far compared%s' % (n, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- PAL

def test_pal(m, c, rep, rnd):
    lib = c.lib
    bad = None
    pal = bytes(rnd.randrange(64) for _ in range(768))
    m.dac[:] = pal
    ctypes.memmove(c.palette, pal, 768)
    n = 0
    for i in range(2000):
        idx = rnd.choice([rnd.randrange(256), 0, 255, 256, -1, 0x1234])
        if rnd.random() < 0.5:
            rgb = [rnd.randrange(0x10000) for _ in range(3)]
            m.dswrite(DS_VEC, struct.pack('<3H', *rgb))
            m.call('PAL_set', [idx, DS_VEC])
            lib.U_PAL_set(s16(idx), (ctypes.c_uint16 * 3)(*rgb))
            a = b = None
        else:
            m.dswrite(DS_VEC, b'\xAA' * 6)
            cv = (ctypes.c_uint16 * 3)(0xAAAA, 0xAAAA, 0xAAAA)
            m.call('PAL_get', [idx, DS_VEC])
            lib.U_PAL_get(s16(idx), cv)
            a, b = m.dsread(DS_VEC, 6), bytes(cv)
        n += 1
        if a != b or bytes(m.dac) != bytes(c.palette):
            bad = 'op %d index %d: words asm %s C %s, DAC %s' % (i, idx, a, b, bytes(m.dac) == bytes(c.palette))
            break
    rep.result('PAL', bad is None, '%d PAL_set/PAL_get compared%s' % (n, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- unit tests

class Clock:
    """osi_stubs' simulated clock: plat_ms stands still but for sleeps and pumps (1 ms)"""

    def __init__(self, lib, ms=100000):
        self.lib = lib
        lib.stub_set_fake_clock(1, ms)
        lib.stub_set_pump_advance(1)
        lib.plat_ms.restype = ctypes.c_uint32
        lib.shim_waits_real()

    def ms(self):
        return self.lib.plat_ms()

    def set(self, ms):
        self.lib.stub_set_fake_clock(1, ms)


def test_timer(c, rep):
    lib = c.lib
    clk = Clock(lib)
    errs = []
    lpt = ctypes.c_int32.in_dll(lib, 'U_loopsPerTick')
    ticks = ctypes.c_int32.in_dll(lib, 'U_timerTicks')
    inst = ctypes.c_int16.in_dll(lib, 'U_timerInstalled')
    t0 = clk.ms()
    lib.U_TIM_calibrate()
    dt = clk.ms() - t0
    if lpt.value != 20000 or inst.value != 0 or not (2 * 54.9 <= dt <= 3 * 54.9 + 2):
        errs.append('TIM_calibrate: loopsPerTick %d, installed %d, %d ms' % (lpt.value, inst.value, dt))
    calls = [0]
    CB = ctypes.CFUNCTYPE(None)

    def cb():
        calls[0] += 1
    cbf = CB(cb)
    lib.U_TIM_install()
    lib.U_TIM_setCallback(cbf)
    start_tick = lib.osi_ticks_live()
    clk.set(clk.ms() + 1000)
    lib.kbhit()
    want = lib.osi_ticks_live() - start_tick
    # one tick a poll: U waits for exact counts (TIM_calibrate), which go up one at a time
    if calls[0] != 1 or ticks.value != 1:
        errs.append('1 s then a poll: %d callbacks, timerTicks %d (1 wanted)' % (calls[0], ticks.value))
    for i in range(want + 3):
        lib.kbhit()
    if calls[0] != want or ticks.value != want:
        errs.append('1 s then polls: %d callbacks, timerTicks %d, %d ticks' % (calls[0], ticks.value, want))
    calls[0] = 0
    clk.set(clk.ms() + 100000)
    lib.U_MOUSE_SERVE()   # (no mouse struct: polls nothing)
    for i in range(40):
        lib.kbhit()
    if not 18 <= calls[0] <= 19:   # (+1: the polls' own idle time can make a tick)
        errs.append('100 s stall: %d callbacks (18 wanted)' % calls[0])
    calls[0] = 0
    t0 = clk.ms()
    lib.U_TIM_delay(2000)
    dt = clk.ms() - t0
    want_ms = 36 * 65536000 / 1193182
    if abs(dt - want_ms) > 2 or not (35 <= calls[0] <= 37):
        errs.append('TIM_delay(2000): %d ms (%.1f wanted), %d callbacks' % (dt, want_ms, calls[0]))
    t0 = clk.ms()
    lib.U_TIM_delay(54)
    if clk.ms() - t0 > 1 or ctypes.c_int32.in_dll(lib, 'U_delayCount').value != 0:
        errs.append('TIM_delay(54) waited %d ms' % (clk.ms() - t0))
    lib.U_TIM_resetCallback()
    lib.U_TIM_remove()
    calls[0] = 0
    clk.set(clk.ms() + 1000)
    lib.kbhit()
    if calls[0] or inst.value:
        errs.append('after TIM_remove: %d callbacks' % calls[0])
    rep.result('TIMER', not errs, 'calibrate, callbacks at poll points, catch-up, TIM_delay%s' % (
        (': ' + '; '.join(errs)) if errs else ''))


def test_vsync(c, rep):
    lib = c.lib
    clk = Clock(lib)
    errs = []
    lib.U_VID_waitRetrace()
    t0 = clk.ms()
    times = []
    for i in range(140):
        lib.U_VID_waitRetrace()
        times.append(clk.ms())
    total = times[-1] - t0
    steps = set(times[i + 1] - times[i] for i in range(len(times) - 1))
    if abs(total - 140 * 1000 / 70.086) > 1.5 or not steps <= {14, 15}:
        errs.append('140 retraces in %d ms, steps %s' % (total, sorted(steps)))
    # VID_inRetrace right after: no wait; once only
    lib.U_VID_waitRetrace()
    t1 = clk.ms()
    lib.U_VID_inRetrace()
    if clk.ms() != t1:
        errs.append('VID_inRetrace right after VID_waitRetrace waited %d ms' % (clk.ms() - t1))
    t1 = clk.ms()
    lib.U_VID_inRetrace()
    if not 13 <= clk.ms() - t1 <= 15:
        errs.append('a second VID_inRetrace waited %d ms' % (clk.ms() - t1))
    rep.result('VSYNC', not errs, '70.086 Hz frame grid, VID_inRetrace after VID_waitRetrace%s' % (
        (': ' + '; '.join(errs)) if errs else ''))


def test_keys(c, rep):
    lib = c.lib
    Clock(lib)
    lib.stub_key_clear()
    errs = []
    seq = []
    if lib.kbhit() != 0:
        errs.append('kbhit with no key')
    for k in (0x1E61, 0x4800, 0x48E0, 0x011B, 0x3B00, 0x2E03):
        lib.stub_key_push(k)
    for _ in range(6):
        seq.append((lib.kbhit(), lib.getch()))
        if seq[-1][1] == 0:
            seq.append((lib.kbhit(), lib.getch()))
    want = [(-1, 0x61), (-1, 0), (-1, 0x48), (-1, 0), (-1, 0x48), (-1, 0x1B), (-1, 0), (-1, 0x3B), (-1, 3)]
    if seq != want:
        errs.append('keys: %s, wanted %s' % (seq, want))
    if lib.kbhit() != 0:
        errs.append('kbhit after all keys')
    lib.stub_key_push(0x1C0D)
    if lib.getch() != 0x0D:
        errs.append('getch without kbhit')
    rep.result('KEYS', not errs, 'kbhit/getch, extended keys%s' % ((': ' + '; '.join(errs)) if errs else ''))


def test_mouse_platform(c, rep):
    lib = c.lib
    Clock(lib)
    errs = []
    cm = CMouse()
    lib.stub_mouse_set(1, 100, 50, 1)
    sem = c.var16('U_nMouseSem')
    r = lib.U_MOUSE_INIT(ctypes.byref(cm))
    if r != 2 or (cm.x, cm.y, cm.buttonL, cm.buttonR, cm.installed, sem.value) != (100, 50, 1, 0, 1, -1):
        errs.append('MOUSE_INIT: %d, %s' % (r, (cm.x, cm.y, cm.buttonL, cm.buttonR, cm.installed, sem.value)))
    if lib.U_MOUSE_SERVE() != 0:
        errs.append('MOUSE_SERVE without a change')
    lib.stub_mouse_set(1, 400, -5, 2)
    if lib.U_MOUSE_SERVE() != 1 or (cm.x, cm.y, cm.buttonL, cm.buttonR) != (319, 0, 0, 2):
        errs.append('MOUSE_SERVE moved: %s' % ((cm.x, cm.y, cm.buttonL, cm.buttonR),))
    lib.stub_mouse_set(0, 0, 0, 0)
    if lib.U_MOUSE_INIT(ctypes.byref(cm)) != 0 or cm.installed or lib.U_MOUSE_SERVE() != 0:
        errs.append('no mouse')
    rep.result('MOUSEP', not errs, 'MOUSE_INIT/SERVE on the platform\'s mouse%s' % (
        (': ' + '; '.join(errs)) if errs else ''))


def test_files(c, rep):
    lib = c.lib
    errs = []
    g_code = ctypes.c_int16.in_dll(lib, 'OSI_ErrorCode')
    u_code = ctypes.c_int16.in_dll(lib, 'U_OSI_ErrorCode')
    u_func = ctypes.c_int16.in_dll(lib, 'U_OSI_ErrorFunc')
    g_handler = ctypes.c_void_p.in_dll(lib, 'OSI_ErrorHandler')
    calls = [0]
    HF = ctypes.CFUNCTYPE(ctypes.c_void_p)

    def handler():
        calls[0] += 1
        return None
    hf = HF(handler)
    old = g_handler.value
    g_handler.value = ctypes.cast(hf, ctypes.c_void_p).value
    g_code.value = 77
    lib.U_OSI_open.argtypes = [ctypes.c_char_p]
    lib.U_OSI_read.argtypes = [ctypes.c_int16, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]
    lib.U_OSI_read.restype = ctypes.c_int32
    h = lib.U_OSI_open(b'NOFILE.XYZ')
    if h != -1 or u_code.value != 2 or u_func.value != 1 or g_code.value != 77 or calls[0]:
        errs.append('OSI_open of a missing file: %d, U error %d/%d, GAME error %d, GAME handler %d' % (
            h, u_code.value, u_func.value, g_code.value, calls[0]))
    h = lib.U_OSI_open(b'u6.set')
    buf = ctypes.create_string_buffer(64)
    n = lib.U_OSI_read(h, 4, 64, buf)
    if h < 5 or n != 64 or buf.raw != open(gamefile('U6.SET'), 'rb').read()[4:68]:
        errs.append('OSI_open/OSI_read: %d, %d' % (h, n))
    if lib.U_OSI_close(h) != 0 or lib.U_OSI_close(h) != -1 or u_code.value != 6 or u_func.value != 2 or calls[0]:
        errs.append('OSI_close twice: U error %d/%d, GAME handler %d' % (u_code.value, u_func.value, calls[0]))
    g_handler.value = old
    g_code.value = 0
    rep.result('FILES', not errs, 'U\'s OSI file routines and error state%s' % ((': ' + '; '.join(errs)) if errs else ''))


# ---------------------------------------------------------------- main

def main():
    global QUICK
    names = [a for a in sys.argv[1:] if not a.startswith('--') and not a.isdigit()]
    seed = 1990
    if '--seed' in sys.argv:
        seed = int(sys.argv[sys.argv.index('--seed') + 1])
    tmp = tempfile.mkdtemp(prefix='uosi-')
    lib = build_lib(tmp)
    overlay = tempfile.mkdtemp(prefix='uosi-overlay-')
    c = CSide(lib, overlay)
    syms = symbols()
    image = load_image()
    drv = driver_image()
    rep = Report()
    diff_tests = [('text', test_text), ('surf', test_surf), ('draw', test_draw), ('mouse', test_mouse),
                  ('fade', test_fade), ('diss', test_dissolve), ('lzw', test_lzw), ('load', test_load),
                  ('pal', test_pal)]
    unit_tests = [('timer', test_timer), ('vsync', test_vsync), ('keys', test_keys),
                  ('mousep', test_mouse_platform), ('files', test_files)]
    for name, fn in diff_tests:
        if names and name not in names:
            continue
        m = Machine(image, syms, drv)
        c.lib.shim_set_wait_cb(c._cb)
        c.lib.stub_set_fake_clock(0, 0)
        try:
            fn(m, c, rep, random.Random('%s-%d' % (name, seed)))
        except (UcError, RuntimeError, AssertionError) as ex:
            rep.result(name.upper(), False, 'exception: %r at %04X:%04X' % (
                ex, m.r(UC_X86_REG_CS), m.r(UC_X86_REG_IP)))
    for name, fn in unit_tests:
        if names and name not in names:
            continue
        try:
            fn(c, rep)
        except (RuntimeError, AssertionError, ValueError) as ex:
            rep.result(name.upper(), False, 'exception: %r' % ex)
    print('%d failed' % rep.failed)
    return 1 if rep.failed else 0


if __name__ == '__main__':
    sys.exit(main())
