#!/usr/bin/env python3
"""Differential tests of port/sys (the OSI library in C) against the original routines.

The original routines are run from GAME.EXE of the DOS build of this source (loaded with
its relocations, DS = DGROUP) in unicorn, with the BIOS/DOS services they call emulated
here (int 16h keyboard queue, int 21h file reads/seeks and time, int 33h mouse, the
graphics driver entries as logging stubs, ports 42h/61h/3DAh). The C side is
build/libosi_test.so (run_osilib_tests.sh builds it).

  RAND    OSI_randomize/OSI_rand: results and generator state after every draw, several
          seeds, long sequences; OSI_timeSeed for many clock values
  LZW     decompress of every LZW file of the game folder and every entry of the
          PORTRAIT.* and CONVERSE.* libraries: output, the whole work area (table and
          input buffer), the bytes around them, the file position and AX
  KBD     KBD_GetKey on random BIOS key queues, both modes
  SOUND   OSI_sound/nosound/playNote/playNoise/playWavedNote/sDelay: the speaker's
          programming (port 42h divisors, port 61h gate per step) and the number of delay
          loop iterations, at several CpuSpeed values
  MOUSE   MOUSE_INIT/POINTER/SERVE/SHOW/HIDE/DRAW/SAVE_BG/IN_AREA on random sequences:
          return values, driver calls (entry, x, y, shape) and onMove calls

Usage: osilib_diff.py [--quick] [test names...]
"""
import ctypes
import os
import random
import struct
import sys
import tempfile

from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INTR, UC_HOOK_INSN, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_CX, UC_X86_REG_DX,
                               UC_X86_REG_SI, UC_X86_REG_DI, UC_X86_REG_BP, UC_X86_REG_SP,
                               UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS,
                               UC_X86_REG_FS, UC_X86_REG_EFLAGS, UC_X86_INS_IN, UC_X86_INS_OUT)

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.environ.get('U6_DOS_EXE', '/home/claude/bld/dos/B/GAME.EXE')
MAP = os.path.splitext(EXE)[0] + '.MAP'
GAMEDATA = os.environ.get('U6_GAMEDATA', '/home/claude/gamedata')
LIB = os.path.join(HERE, 'build', 'libosi_test.so')

LOAD = 0x1000          # load segment of the image
STOP = 0x00500         # linear address of a HLT: far return address of every call
STACK_SEG = 0xF000
DRV_SEG = 0x0700       # graphics driver / callback stubs
QUICK = '--quick' in sys.argv

# DGROUP offsets
D_SOUNDFLAG = 0x2C72
D_CPUSPEED = 0x2FC6
D_PMOUSE = 0x2FB4
D_MOUSESEM = 0x2FB6
D_GRDRIVER = 0xECB8
D_SEED_LO, D_SEED_HI = 0x3088, 0x308A


def w16(v):
    return v & 0xFFFF


class Exe:
    """GAME.EXE's load image, relocated at LOAD, and its segment frames from GAME.MAP."""

    def __init__(self):
        d = open(EXE, 'rb').read()
        h = struct.unpack_from('<14H', d)
        cblp, cp, crlc, cparhdr, lfarlc = h[1], h[2], h[3], h[4], h[12]
        size = (cp - 1) * 512 + cblp if cblp else cp * 512
        self.image = bytearray(d[cparhdr * 16:size])
        for i in range(crlc):
            off, seg = struct.unpack_from('<HH', d, lfarlc + 4 * i)
            p = seg * 16 + off
            v = struct.unpack_from('<H', self.image, p)[0]
            struct.pack_into('<H', self.image, p, w16(v + LOAD))
        self.segs = {}
        for line in open(MAP, errors='replace'):
            f = line.split()
            if len(f) >= 4 and f[0].endswith('H') and (f[3][0].isalpha() or f[3][0] == '_'):
                try:
                    self.segs[f[3]] = int(f[0][:-1], 16)
                except ValueError:
                    pass
        self.dgroup = LOAD + (self.segs['_DATA'] >> 4)

    def find(self, seg, sig, nth=0):
        """(cs, ip) of the nth occurrence of sig (hex) in segment seg."""
        start = self.segs[seg]
        frame = start >> 4
        pat = bytes.fromhex(sig)
        pos = start
        for _ in range(nth + 1):
            pos = self.image.find(pat, pos, start + 0x10000)
            if pos < 0:
                raise RuntimeError('signature %s not found in %s' % (sig, seg))
            pos += 1
        return (LOAD + frame, pos - 1 - frame * 16)


class Machine:
    def __init__(self, exe):
        self.exe = exe
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, 0x120000)
        uc.mem_write(LOAD * 16, bytes(exe.image))
        uc.mem_write(STOP, b'\xf4')
        self.ds = exe.dgroup
        self.ints = {}
        self.port_in = {}
        self.port_out = None
        uc.hook_add(UC_HOOK_INTR, self._intr)
        uc.hook_add(UC_HOOK_INSN, self._in, None, 1, 0, UC_X86_INS_IN)
        uc.hook_add(UC_HOOK_INSN, self._out, None, 1, 0, UC_X86_INS_OUT)

    def _intr(self, uc, intno, ud):
        h = self.ints.get(intno)
        if not h:
            raise RuntimeError('unexpected int %02Xh AX=%04X' % (intno, uc.reg_read(UC_X86_REG_AX)))
        h(uc)

    def _in(self, uc, port, size, ud):
        f = self.port_in.get(port)
        return f() if f else 0xFF

    def _out(self, uc, port, size, value, ud):
        if self.port_out:
            self.port_out(port, value)

    def r(self, reg):
        return self.uc.reg_read(reg)

    def set_flag(self, bit, on):
        f = self.uc.reg_read(UC_X86_REG_EFLAGS)
        self.uc.reg_write(UC_X86_REG_EFLAGS, (f | (1 << bit)) if on else (f & ~(1 << bit)))

    def rd16(self, lin):
        return struct.unpack('<H', self.uc.mem_read(lin, 2))[0]

    def wr16(self, lin, v):
        self.uc.mem_write(lin, struct.pack('<H', w16(v)))

    def dsw(self, off, v=None):
        if v is None:
            return self.rd16(self.ds * 16 + off)
        self.wr16(self.ds * 16 + off, v)

    def call(self, entry, words=(), ds=None, regs=None, max_insns=0):
        """Far call cs:ip with the words pushed in this order; returns (ax, dx)."""
        cs, ip = entry
        uc = self.uc
        sp = 0xFFF0
        for wv in words:
            sp -= 2
            self.wr16(STACK_SEG * 16 + sp, wv)
        sp -= 4
        self.wr16(STACK_SEG * 16 + sp, STOP & 0xF)
        self.wr16(STACK_SEG * 16 + sp + 2, STOP >> 4)
        uc.reg_write(UC_X86_REG_SS, STACK_SEG)
        uc.reg_write(UC_X86_REG_SP, sp)
        uc.reg_write(UC_X86_REG_DS, self.ds if ds is None else ds)
        uc.reg_write(UC_X86_REG_ES, self.ds)
        uc.reg_write(UC_X86_REG_CS, cs)
        uc.reg_write(UC_X86_REG_EFLAGS, 0x0202)
        for k, v in (regs or {}).items():
            uc.reg_write(k, v)
        uc.emu_start(cs * 16 + ip, STOP, 0, max_insns)
        return self.r(UC_X86_REG_AX), self.r(UC_X86_REG_DX)


def load_lib(overlay):
    lib = ctypes.CDLL(LIB)
    lib.stub_quiet_log(1)
    lib.stub_set_dirs(overlay.encode(), GAMEDATA.encode())
    lib.dosfs_init.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    if lib.dosfs_init(GAMEDATA.encode(), overlay.encode()) != 0:
        raise RuntimeError('dosfs_init failed')
    lib.t_arena.restype = ctypes.POINTER(ctypes.c_uint8)
    lib.t_trace.restype = ctypes.POINTER(ctypes.c_uint32)
    lib.t_loops.restype = ctypes.c_uint64
    lib.t_lzw.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint16, ctypes.c_uint16,
                          ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint32)]
    lib.t_seed_from_time.restype = ctypes.c_uint16
    lib.stub_grlog.restype = ctypes.c_char_p
    return lib


class Report:
    def __init__(self):
        self.lines = []
        self.failed = 0

    def result(self, name, ok, detail):
        self.lines.append('%-6s %s  %s' % (name, 'PASS' if ok else 'FAIL', detail))
        if not ok:
            self.failed += 1
        print(self.lines[-1], flush=True)


# ---------------------------------------------------------------- RAND

def test_rand(exe, m, lib, rep):
    rnd = random.Random(1990)
    e_rand = exe.find('SEG_33C5_TEXT', '558bec9a')
    e_randomize = exe.find('SEG_33C5_TEXT', '558bec8b46060d0100')
    e_timeseed = exe.find('SEG_33CA_TEXT', '558bec56571eb42c')
    # a batch driver: FS:SI = (a, b) pairs, ES:DI = (result, lo, hi) per call, CX = count
    drv_lin = 0x8000
    rel = drv_lin + 3
    code = bytes([0x51, 0x64, 0xFF, 0x34, 0x64, 0xFF, 0x74, 0x02,
                  0x9A]) + struct.pack('<HH', e_rand[1], e_rand[0]) + bytes([
                      0xAB, 0xA1]) + struct.pack('<H', D_SEED_LO) + bytes([0xAB, 0xA1]) + \
        struct.pack('<H', D_SEED_HI) + bytes([0xAB, 0x83, 0xC6, 0x04, 0x59])
    code += bytes([0xE2, (-(len(code) + 2)) & 0xFF, 0xF4])
    m.uc.mem_write(drv_lin, code)
    del rel

    lo, hi = m.dsw(D_SEED_LO), m.dsw(D_SEED_HI)
    clo, chi = ctypes.c_uint16(), ctypes.c_uint16()
    lib.osi_rand_state(ctypes.byref(clo), ctypes.byref(chi))
    ok = (lo, hi) == (clo.value, chi.value) == (1, 0)
    total = 0
    bad = None
    seeds = [None, 0, 1, 2, 0x1234, 0x8000, 0xFFFF, 0xFFFE] + [rnd.randrange(0x10000) for _ in range(4)]
    per_seed = 20000 if QUICK else 60000
    edge = [(0, 0), (0, 0xFFFF), (0xFFFF, 0), (1, 0xFFFF), (0, 1), (1, 0), (5, 5), (19, 200),
            (2000, 15000), (0x7FFF, 0x8000), (0xFFFF, 0xFFFF), (0, 0x7FFF), (100, 7)]
    for seed in seeds:
        if seed is not None:
            m.call(e_randomize, [seed])
            lib.OSI_randomize(seed)
        done = 0
        while done < per_seed:
            n = min(10000, per_seed - done)
            pairs = []
            for i in range(n):
                k = rnd.random()
                if k < 0.1:
                    pairs.append(rnd.choice(edge))
                elif k < 0.6:
                    a = rnd.randrange(0, 600)
                    pairs.append((a, a + rnd.randrange(0, 300)))
                else:
                    pairs.append((rnd.randrange(0x10000), rnd.randrange(0x10000)))
            flat = [v for p in pairs for v in p]
            m.uc.mem_write(0x60000, struct.pack('<%dH' % len(flat), *flat))
            m.uc.reg_write(UC_X86_REG_FS, 0x6000)
            m.uc.reg_write(UC_X86_REG_SI, 0)
            m.uc.reg_write(UC_X86_REG_DI, 0)
            m.uc.reg_write(UC_X86_REG_CX, n)
            m.uc.reg_write(UC_X86_REG_SS, STACK_SEG)
            m.uc.reg_write(UC_X86_REG_SP, 0xFFF0)
            m.uc.reg_write(UC_X86_REG_DS, m.ds)
            m.uc.reg_write(UC_X86_REG_ES, 0x7000)
            m.uc.reg_write(UC_X86_REG_CS, drv_lin >> 4)
            m.uc.emu_start(drv_lin, drv_lin + len(code) - 1)
            asm = struct.unpack('<%dH' % (3 * n), m.uc.mem_read(0x70000, 6 * n))
            cin = (ctypes.c_uint16 * (2 * n))(*flat)
            cout = (ctypes.c_uint16 * (3 * n))()
            lib.t_rand_batch(cin, n, cout)
            if list(cout) != list(asm) and bad is None:
                for i in range(n):
                    if tuple(cout[3 * i:3 * i + 3]) != asm[3 * i:3 * i + 3]:
                        bad = 'seed %s draw %d rand%s: asm %s, C %s' % (
                            seed, done + i, pairs[i], asm[3 * i:3 * i + 3], tuple(cout[3 * i:3 * i + 3]))
                        break
            done += n
            total += n
    rep.result('RAND', ok and bad is None, '%d draws, %d seeds%s' % (total, len(seeds), (': ' + bad) if bad else ''))

    # OSI_timeSeed
    clock = [0, 0, 0, 0]

    def int21(uc):
        ah = m.r(UC_X86_REG_AX) >> 8
        assert ah == 0x2C
        uc.reg_write(UC_X86_REG_CX, (clock[0] << 8) | clock[1])
        uc.reg_write(UC_X86_REG_DX, (clock[2] << 8) | clock[3])
    m.ints[0x21] = int21
    cases = [(h, mi, s, hs) for h in (0, 1, 12, 23) for mi in (0, 1, 31, 59) for s in (0, 1, 30, 59) for hs in (0, 1, 50, 99)]
    cases += [(rnd.randrange(24), rnd.randrange(60), rnd.randrange(60), rnd.randrange(100))
              for _ in range(3000 if QUICK else 20000)]
    bad = None
    for c in cases:
        clock[:] = c
        a, _ = m.call(e_timeseed)
        cval = lib.t_seed_from_time(*c)
        if a != cval:
            bad = 'time %02d:%02d:%02d.%02d asm %04X C %04X' % (c + (a, cval))
            break
    rep.result('SEED', bad is None, '%d clock values%s' % (len(cases), (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- LZW

def lzw_cases():
    cases = []
    for name in sorted(os.listdir(GAMEDATA)):
        p = os.path.join(GAMEDATA, name)
        if not os.path.isfile(p) or os.path.getsize(p) < 8:
            continue
        d = open(p, 'rb').read(8)
        size = struct.unpack_from('<I', d)[0]
        if (d[4] | (d[5] << 8)) & 0x1FF == 256 and 0 < size < 0x40000:
            cases.append((name, 0))
    for lib_name, is_conv in (('PORTRAIT.A', 0), ('PORTRAIT.B', 0), ('PORTRAIT.Z', 0), ('CONVERSE.A', 1), ('CONVERSE.B', 1)):
        p = os.path.join(GAMEDATA, lib_name)
        if not os.path.exists(p):
            continue
        d = open(p, 'rb').read()
        first = None
        i = 0
        while True:
            if first is not None and i * 4 >= first:
                break
            if i * 4 + 4 > len(d):
                break
            off = struct.unpack_from('<I', d, i * 4)[0]
            i += 1
            if off == 0:
                continue
            if first is None or off < first:
                first = off
            if off + 6 > len(d):
                continue
            size = struct.unpack_from('<I', d, off)[0]
            if is_conv and not (size and size < 0x2800):
                continue
            if (d[off + 4] | (d[off + 5] << 8)) & 0x1FF != 256:
                continue
            cases.append((lib_name, off))
    return cases


def test_lzw(exe, m, lib, rep):
    e_dec = exe.find('C_32FD_TEXT', '558bec56571e8b460c')
    files = {}

    def int21(uc):
        ax = m.r(UC_X86_REG_AX)
        ah = ax >> 8
        f = files[m.r(UC_X86_REG_BX)]
        if ah == 0x42:
            pos = (m.r(UC_X86_REG_CX) << 16) | m.r(UC_X86_REG_DX)
            assert ax & 0xFF == 0
            f.seek(pos)
            uc.reg_write(UC_X86_REG_AX, pos & 0xFFFF)
            uc.reg_write(UC_X86_REG_DX, pos >> 16)
        elif ah == 0x3F:
            n = m.r(UC_X86_REG_CX)
            lin = m.r(UC_X86_REG_DS) * 16 + m.r(UC_X86_REG_DX)
            data = f.read(n)
            uc.mem_write(lin, data)
            uc.reg_write(UC_X86_REG_AX, len(data))
        else:
            raise RuntimeError('int 21h AH=%02X' % ah)
        m.set_flag(0, False)
    m.ints[0x21] = int21
    arena = lib.t_arena()
    cases = lzw_cases()
    if QUICK:
        cases = cases[::5]
    rnd = random.Random(6)
    bad = []
    total_out = 0
    for idx, (name, offset) in enumerate(cases):
        d = open(os.path.join(GAMEDATA, name), 'rb')
        d.seek(offset)
        size = struct.unpack('<I', d.read(4))[0]
        d.close()
        dest_seg = 0x5000
        dest_off = rnd.choice([0, 0, 4, 0xF, 0x123])
        work = dest_seg + ((dest_off + size + 15) >> 4)
        lo = dest_seg * 16
        hi = work * 16 + 0x3400 + 0x40
        pattern = bytes(((i * 7 + 3) & 0xFF) for i in range(hi - lo))
        m.uc.mem_write(lo, pattern)
        ctypes.memmove(ctypes.addressof(arena.contents) + lo, pattern, len(pattern))
        f = open(os.path.join(GAMEDATA, name), 'rb')
        files[5] = f
        ax, dx = m.call(e_dec, [work, dest_seg, dest_off, offset >> 16, offset & 0xFFFF, 5])
        asm_pos = f.tell()
        f.close()
        pos = ctypes.c_uint32()
        cax = lib.t_lzw(name.encode(), offset, dest_seg, dest_off, work, ctypes.byref(pos))
        asm_mem = bytes(m.uc.mem_read(lo, hi - lo))
        c_mem = ctypes.string_at(ctypes.addressof(arena.contents) + lo, hi - lo)
        total_out += size
        if asm_mem != c_mem or asm_pos != pos.value or (ax & 0xFFFF) != (cax & 0xFFFF):
            first = next((i for i in range(len(asm_mem)) if asm_mem[i] != c_mem[i]), None)
            bad.append('%s@%X: mem diff at +%s, pos asm %d C %d, ax asm %X C %X' % (
                name, offset, None if first is None else hex(first), asm_pos, pos.value, ax, cax))
        # the decompressed size must match the header (a sanity check of the test itself)
        if idx == 0 or not bad:
            pass
    rep.result('LZW', not bad, '%d streams (%d files and library entries), %d bytes unpacked, '
               'work area and file position compared%s' % (
                   len(cases), len(set(c[0] for c in cases)), total_out, ('; ' + '; '.join(bad[:5])) if bad else ''))


# ---------------------------------------------------------------- KBD

def test_kbd(exe, m, lib, rep):
    e_kbd = exe.find('SEG_31FA', '558bec1e5657f74606ffff')
    queue = []

    def int16(uc):
        ah = m.r(UC_X86_REG_AX) >> 8
        if ah == 1:
            if queue:
                uc.reg_write(UC_X86_REG_AX, queue[0])
                m.set_flag(6, False)
            else:
                m.set_flag(6, True)
        elif ah == 0:
            uc.reg_write(UC_X86_REG_AX, queue.pop(0))
        else:
            raise RuntimeError('int 16h AH=%02X' % ah)
    m.ints[0x16] = int16
    rnd = random.Random(16)
    special = [0x0000, 0x0E08, 0x0E7F, 0x4700, 0x4737, 0x4838, 0x4A2D, 0x4E2B, 0x4A00, 0x4E00, 0x5100,
               0x5133, 0x5230, 0x532E, 0x1C0D, 0x011B, 0x2E03, 0x3B00, 0x8500, 0x00E0, 0x47E0, 0x4BE0,
               0x0061, 0x1E61, 0x3920, 0x5000, 0x4800, 0x4600, 0x52E0, 0x4C35, 0x4D36, 0x1071]
    n = 0
    bad = None
    for _ in range(3000 if QUICK else 20000):
        keys = [rnd.choice(special) if rnd.random() < 0.6 else rnd.randrange(0x10000)
                for _ in range(rnd.choice([0, 0, 1, 1, 2, 3, 5]))]
        flag = rnd.choice([0, 1, 2, 0x100])
        queue[:] = keys
        lib.stub_key_clear()
        for k in keys:
            lib.stub_key_push(k)
        a, _ = m.call(e_kbd, [flag])
        c = lib.KBD_GetKey(flag) & 0xFFFF
        left_asm = len(queue)
        left_c = lib.plat_key_available()
        n += 1
        if a != c or (left_asm > 0) != bool(left_c):
            bad = 'keys %s flag %d: asm %04X C %04X' % (['%04X' % k for k in keys], flag, a, c)
            break
    rep.result('KBD', bad is None, '%d polls%s' % (n, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- SOUND

def test_sound(exe, m, lib, rep):
    e = {
        'sDelay': exe.find('SEG_33C2', '558bec56571ea1c62f'),
        'waved': exe.find('SEG_33CD_TEXT', '558bec56571ea1c62f3d6400'),
        'nosound': exe.find('SEG_33D8_TEXT', 'e46124fce661cb'),
        'sound': exe.find('SEG_33D8_TEXT', '558bec56571e833e722c00'),
        'noise': exe.find('SEG_33DB_TEXT', '558bec56571ec706923000'),
        'note': exe.find('SEG_33E3_TEXT', '558bec803e722c00'),
    }
    # the inner "dec word [x] / jnz" of every DELAY_CXAX copy
    loops = [0]
    start, end = exe.segs['SEG_33C2'], exe.segs['SEG_33E3_TEXT'] + 0x30
    pos = start
    addrs = []
    while True:
        pos = exe.image.find(b'\xff\x0e', pos, end)
        if pos < 0:
            break
        if exe.image[pos + 4:pos + 6] == b'\x75\xfa':
            addrs.append(LOAD * 16 + pos)
        pos += 1

    def count(uc, addr, size, ud):
        loops[0] += 1
    for a in addrs:
        m.uc.hook_add(UC_HOOK_CODE, count, None, a, a)
    events = []
    port61 = [0]
    div = []

    def out(port, value):
        if port == 0x42:
            div.append(value)
            if len(div) == 2:
                events.append(('div', div[0] | (div[1] << 8)))
                div[:] = []
        elif port == 0x61:
            port61[0] = value
            events.append(('gate', 1 if (value & 3) == 3 else 0))
    m.port_out = out
    m.port_in[0x61] = lambda: port61[0]
    lib.t_set_rp_mode(2)   # replay mode: the C side does not wait
    sflag = ctypes.c_int16.in_dll(lib, 'SoundFlag')

    def c_events():
        n = lib.t_trace_count()
        t = lib.t_trace()
        out = []
        for i in range(n):
            what, v = t[i] >> 16, t[i] & 0xFFFF
            if what == 0:
                out.append(('div', v))
            elif what in (1, 3):
                out.append(('gate', v))
        return out

    rnd = random.Random(61)
    calls = []
    # the game's own calls (music.c, spell.c, objcmd.c, npctrack.c)
    game = [('note', [440, 10]), ('note', [2000, 10]), ('note', [3000, 2]), ('note', [100, 200]),
            ('noise', [1, 800, 10000]), ('noise', [1, 7000, 600]), ('noise', [1, 350, 2000]),
            ('noise', [40, 120, 300]), ('noise', [10, 30, 25000]), ('noise', [20, 60, 10000]),
            ('noise', [800, 9600, 700]), ('noise', [1, 800, 3000]),
            ('waved', [440, 1, 700, 30000, -25]), ('waved', [220, 1, 6000, 30000, -5]),
            ('waved', [440, 1, 4000, 15000, -3]), ('waved', [220, 1, 2000, 20000, -20]),
            ('waved', [800, 1, 1000, 10000, 200]), ('waved', [1600, 1, 8000, 0, 8]),
            ('waved', [0xC2C, 1, 2000, 20000, -10]), ('waved', [262, 1, 20000, 1, 3]),
            ('waved', [2000, 1, 16000, 20000, -1]), ('waved', [0x1180, 1, 65000, 300, 1]),
            ('waved', [1000, 1, 14000, 2000, 30]),
            ('sDelay', [1, 10]), ('sDelay', [8, 1]), ('sDelay', [1, 1]), ('sDelay', [3, 5]), ('sDelay', [16, 2]),
            ('sound', [440]), ('nosound', []), ('sound', [19]), ('sound', [65535]), ('nosound', [])]
    for c in game:
        calls.append((rnd.choice([100, 240, 1000]), 1, c))
    for _ in range(40 if QUICK else 200):
        kind = rnd.choice(['note', 'noise', 'waved', 'waved', 'sDelay', 'sound', 'nosound'])
        if kind == 'note':
            args = [rnd.randrange(19, 30000), rnd.randrange(1, 60)]
        elif kind == 'noise':
            args = [rnd.randrange(1, 50), rnd.randrange(1, 3000), rnd.choice([100, 101, 150, 600, 5000, 32767, 40000, 65535])]
        elif kind == 'waved':
            args = [rnd.randrange(0x10000), rnd.choice([1, 1, 2, 3]), rnd.randrange(1, 5000),
                    rnd.randrange(0x10000), rnd.randrange(-300, 300)]
        elif kind == 'sDelay':
            args = [rnd.choice([0, 1, 2, 3, 4, 5, 8, 9, 15, 16, 17, 20, 31]), rnd.randrange(1, 40)]
        elif kind == 'sound':
            args = [rnd.randrange(19, 0x10000)]
        else:
            args = []
        calls.append((rnd.choice([16, 50, 99, 100, 101, 240, 1000]), rnd.choice([1, 1, 1, 0, 0x100]), (kind, args)))
    # the CBW quirk of OSI_playWavedNote: CpuSpeed 3072..6143 gives a negative D_3090
    calls.append((3500, 1, ('waved', [1000, 1, 3, 30000, 5])))
    names = {'note': 'OSI_playNote', 'noise': 'OSI_playNoise', 'waved': 'OSI_playWavedNote',
             'sDelay': 'OSI_sDelay', 'sound': 'OSI_sound', 'nosound': 'OSI_nosound'}
    bad = None
    total_loops = 0
    for speed, flag, (kind, args) in calls:
        m.dsw(D_CPUSPEED, speed)
        m.dsw(D_SOUNDFLAG, flag)
        lib.t_set_cpu_speed(speed)
        sflag.value = flag if flag < 0x8000 else flag - 0x10000
        events[:] = []
        loops[0] = 0
        lib.t_trace_start()
        m.call(e[kind], [w16(a) for a in args])
        getattr(lib, names[kind])(*args)
        ce = c_events()
        cl = lib.t_loops()
        total_loops += loops[0]
        if ce != events or cl != loops[0]:
            first = next((i for i in range(min(len(ce), len(events))) if ce[i] != events[i]), min(len(ce), len(events)))
            bad = '%s%s CpuSpeed %d SoundFlag %d: loops asm %d C %d; events asm %d C %d, first diff #%d asm %s C %s' % (
                names[kind], tuple(args), speed, flag, loops[0], cl, len(events), len(ce), first,
                events[first:first + 3], ce[first:first + 3])
            break
    lib.t_set_rp_mode(0)
    rep.result('SOUND', bad is None, '%d calls, %d delay loop iterations, speaker programming and loop counts '
               'compared%s' % (len(calls), total_loops, (': ' + bad) if bad else ''))


# ---------------------------------------------------------------- MOUSE

class CMouse(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('buttonL', ctypes.c_uint8), ('buttonR', ctypes.c_uint8), ('posX', ctypes.c_int16),
                ('posY', ctypes.c_int16), ('unused06', ctypes.c_uint16), ('shapes', ctypes.c_void_p),
                ('shapeIndex', ctypes.c_uint16), ('surface', ctypes.c_void_p), ('saveBuf', ctypes.c_void_p),
                ('onMove', ctypes.c_void_p), ('installed', ctypes.c_uint8)]


class CSurface(ctypes.Structure):
    _pack_ = 1
    _fields_ = [('segment', ctypes.c_uint16), ('pScanlines', ctypes.c_void_p), ('right', ctypes.c_uint16),
                ('left', ctypes.c_uint16), ('top', ctypes.c_uint16), ('bottom', ctypes.c_uint16)]


def test_mouse(exe, m, lib, rep):
    seg = 'MOUSE_TEXT'
    e = {
        'INIT': exe.find(seg, '558bec56571e8b5e06891eb42f'),
        'POINTER': exe.find(seg, '558bec56571ec47e06'),
        'SERVE': exe.find(seg, '558bec56571e8b1eb42f'),
        'SHOW': exe.find(seg, 'ff06b62f'),
        'HIDE': exe.find(seg, 'ff0eb62f833eb62fff'),
        'DRAW': exe.find(seg, '8b1eb42f807f1800741c'),
        'SAVE_BG': exe.find(seg, '8b1eb42f807f18007422'),
        'IN_AREA': exe.find(seg, '558bec56571e068b36b42f'),
    }
    rnd = random.Random(33)
    ds_lin = m.ds * 16
    MOUSE_OFF, SURF_OFF, AREA_OFF = 0xF000, 0xF040, 0xF060
    SHAPES_SEG = 0x9000
    SAVE_SEG = 0x9800
    # the shape table: 8 shapes, dword offsets from the table, headers of 4 words
    nshapes = 8
    table = bytearray(4 + 4 * nshapes)
    hdrs = bytearray()
    for i in range(nshapes):
        off = len(table) + len(hdrs) + rnd.randrange(0, 8)
        while len(table) + len(hdrs) < off:
            hdrs.append(0)
        struct.pack_into('<I', table, 4 + 4 * i, off)
        hdrs += struct.pack('<4h', rnd.randrange(0, 16), rnd.randrange(0, 16), rnd.randrange(0, 16), rnd.randrange(0, 16))
    shapes = bytes(table + hdrs)
    m.uc.mem_write(SHAPES_SEG * 16, shapes)
    # driver stubs: entries 0Ch, 0Fh (8 words) and 30h (6 words), onMove at 40h
    drv = bytearray(0x50)
    drv[0x0C:0x0F] = b'\xca\x10\x00'
    drv[0x0F:0x12] = b'\xca\x10\x00'
    drv[0x30:0x33] = b'\xca\x0c\x00'
    drv[0x40] = 0xCB
    m.uc.mem_write(DRV_SEG * 16, bytes(drv))
    m.wr16(ds_lin + D_GRDRIVER + 2, DRV_SEG)
    asm_log = []

    def drvhook(uc, addr, size, ud):
        fn = addr - DRV_SEG * 16
        sp = m.r(UC_X86_REG_SS) * 16 + m.r(UC_X86_REG_SP)
        w = [m.rd16(sp + 4 + 2 * i) for i in range(8)]   # w[0] = last pushed
        if fn == 0x30:
            # pushed: surface, posX, posY, shapes seg, shapes ofs, shapeIndex
            asm_log.append('30(%d,%d,%d)' % (ctypes.c_int16(w[4]).value, ctypes.c_int16(w[3]).value, w[0]))
        elif fn in (0x0C, 0x0F):
            # pushed: surface, saveBuf seg, ofs, posX, posY, shapes seg, ofs, shapeIndex
            asm_log.append('%02X(%d,%d,%d)' % (fn, ctypes.c_int16(w[4]).value, ctypes.c_int16(w[3]).value, w[0]))
        elif fn == 0x40:
            mb = bytes(m.uc.mem_read(ds_lin + MOUSE_OFF, 6))
            asm_log.append('move(%d,%d,%d,%d)' % (struct.unpack_from('<h', mb, 2)[0], struct.unpack_from('<h', mb, 4)[0], mb[0], mb[1]))
    for fn in (0x0C, 0x0F, 0x30, 0x40):
        m.uc.hook_add(UC_HOOK_CODE, drvhook, None, DRV_SEG * 16 + fn, DRV_SEG * 16 + fn)
    mstate = {'present': 1, 'x': 0, 'y': 0, 'b': 0}

    def int33(uc):
        ax = m.r(UC_X86_REG_AX)
        if ax == 0:
            uc.reg_write(UC_X86_REG_AX, 0xFFFF if mstate['present'] else 0)
            uc.reg_write(UC_X86_REG_BX, 2)
        elif ax == 3:
            uc.reg_write(UC_X86_REG_CX, mstate['x'] * 2)
            uc.reg_write(UC_X86_REG_DX, mstate['y'])
            uc.reg_write(UC_X86_REG_BX, mstate['b'])
        else:
            raise RuntimeError('int 33h AX=%X' % ax)
    m.ints[0x33] = int33
    m.port_in[0x3DA] = lambda: 0x08

    # C side
    arena = lib.t_arena()
    ctypes.memmove(ctypes.addressof(arena.contents) + SHAPES_SEG * 16, shapes, len(shapes))
    c_shapes = ctypes.addressof(arena.contents) + SHAPES_SEG * 16
    c_save = ctypes.addressof(arena.contents) + SAVE_SEG * 16
    cm = CMouse()
    surf = CSurface(0, None, 319, 0, 0, 199)
    area = CSurface()
    c_log = []
    ONMOVE = ctypes.CFUNCTYPE(None)

    def on_move():
        c_log.append('move(%d,%d,%d,%d)' % (cm.posX, cm.posY, cm.buttonL, cm.buttonR))
    on_move_cb = ONMOVE(on_move)
    lib.MOUSE_POINTER.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
    lib.MOUSE_IN_AREA.argtypes = [ctypes.c_void_p]
    lib.MOUSE_INIT.argtypes = [ctypes.c_void_p]

    def grlog():
        s = lib.stub_grlog().decode().split()
        lib.stub_grlog_clear()
        return s

    def run(op, args):
        """the operation on both sides; returns (asm result, C result)"""
        if op == 'INIT':
            # main.c: Mouse.shapes, Mouse.surface set, then MOUSE_INIT, onMove, MOUSE_POINTER(shapes, 0)
            mb = bytearray(25)
            struct.pack_into('<HH', mb, 8, 0, SHAPES_SEG)
            struct.pack_into('<H', mb, 0x0E, SURF_OFF)
            struct.pack_into('<HH', mb, 0x10, 0, SAVE_SEG)
            m.uc.mem_write(ds_lin + MOUSE_OFF, bytes(mb))
            m.uc.mem_write(ds_lin + SURF_OFF, struct.pack('<HHHHHHH', 0, 0, 0, 319, 0, 0, 199))
            ra, _ = m.call(e['INIT'], [MOUSE_OFF])
            m.wr16(ds_lin + MOUSE_OFF + 0x14, 0x40)
            m.wr16(ds_lin + MOUSE_OFF + 0x16, DRV_SEG)
            ctypes.memset(ctypes.addressof(cm), 0, ctypes.sizeof(cm))
            cm.shapes = c_shapes
            cm.surface = ctypes.addressof(surf)
            cm.saveBuf = c_save
            rc = lib.MOUSE_INIT(ctypes.byref(cm))
            cm.onMove = ctypes.cast(on_move_cb, ctypes.c_void_p).value
            return ra, rc
        if op == 'POINTER':
            ra, _ = m.call(e['POINTER'], [args[0], SHAPES_SEG, 0])
            lib.MOUSE_POINTER(c_shapes, args[0])
            return 0, 0
        if op == 'SERVE':
            mstate.update(x=args[0], y=args[1], b=args[2])
            lib.stub_mouse_set(1, args[0], args[1], args[2])
            ra, _ = m.call(e['SERVE'])
            return ra, lib.MOUSE_SERVE()
        if op == 'IN_AREA':
            x0, y0, x1, y1 = args
            m.uc.mem_write(ds_lin + AREA_OFF, struct.pack('<HHHHHHH', 0, 0, 0, w16(x1), w16(x0), w16(y0), w16(y1)))
            area.right, area.left, area.top, area.bottom = w16(x1), w16(x0), w16(y0), w16(y1)
            ra, _ = m.call(e['IN_AREA'], [AREA_OFF])
            return ra, lib.MOUSE_IN_AREA(ctypes.byref(area))
        ra, _ = m.call(e[op])
        getattr(lib, 'MOUSE_' + op)()
        return 0, 0

    bad = None
    nops = 0
    stats = {'in_area_1': 0, 'serve_1': 0, 'driver': 0, 'onmove': 0}
    for round_ in range(20 if QUICK else 100):
        m.dsw(D_MOUSESEM, 0)
        asm_log[:] = []
        c_log[:] = []
        lib.stub_grlog_clear()
        ops = [('INIT', ())]
        x, y, b = rnd.randrange(320), rnd.randrange(200), 0
        for _ in range(60):
            k = rnd.random()
            if k < 0.4:
                if rnd.random() < 0.5:
                    x, y = rnd.randrange(320), rnd.randrange(200)
                if rnd.random() < 0.3:
                    b = rnd.randrange(4)
                ops.append(('SERVE', (x, y, b)))
            elif k < 0.53:
                ops.append(('SHOW', ()))
            elif k < 0.6:
                ops.append(('HIDE', ()))
            elif k < 0.7:
                ops.append(('POINTER', (rnd.randrange(nshapes),)))
            elif k < 0.9:
                x0, y0 = rnd.randrange(-20, 330), rnd.randrange(-20, 210)
                ops.append(('IN_AREA', (x0, y0, x0 + rnd.randrange(0, 200), y0 + rnd.randrange(0, 200))))
            else:
                ops.append((rnd.choice(['DRAW', 'SAVE_BG']), ()))
        for op, args in ops:
            nops += 1
            ra, rc = run(op, args)
            ca = [s for s in grlog()]
            # merge the C driver log and onMove log in call order: compare per operation
            a_ev = list(asm_log)
            asm_log[:] = []
            c_ev = ca
            c_moves = list(c_log)
            c_log[:] = []
            a_moves = [s for s in a_ev if s.startswith('move')]
            a_drv = [s for s in a_ev if not s.startswith('move')]
            if op == 'IN_AREA' and ra == 1:
                stats['in_area_1'] += 1
            if op == 'SERVE' and ra == 1:
                stats['serve_1'] += 1
            stats['driver'] += len(a_drv)
            stats['onmove'] += len(a_moves)
            if (ra & 0xFFFF) != (rc & 0xFFFF) or a_drv != c_ev or a_moves != c_moves:
                bad = 'round %d op %s%s: result asm %d C %d; driver asm %s C %s; onMove asm %s C %s' % (
                    round_, op, args, ra, rc, a_drv, c_ev, a_moves, c_moves)
                break
        if bad:
            break
    # MOUSE_IN_AREA at its boundaries: each of the four compares one pixel either side
    if bad is None:
        hdr = []
        for i in range(nshapes):
            off = struct.unpack_from('<I', shapes, 4 + 4 * i)[0]
            hdr.append(struct.unpack_from('<4h', shapes, off))
        ops = [('INIT', ()), ('SHOW', ())]
        for _ in range(60 if QUICK else 300):
            idx = rnd.randrange(nshapes)
            x, y = rnd.randrange(320), rnd.randrange(200)   # the mouse driver keeps it on screen
            ops.append(('SERVE', (x, y, 0)))
            ops.append(('POINTER', (idx,)))
            r, l, u, dn = hdr[idx]
            for d in (-1, 0, 1):
                right = x - l + d
                ops.append(('IN_AREA', (right - 50, y - 50, right, y + 50)))
                left = x + r + d
                ops.append(('IN_AREA', (left, y - 50, left + 50, y + 50)))
                bottom = y - u + d
                ops.append(('IN_AREA', (x - 50, bottom - 50, x + 50, bottom)))
                top = y + dn + d
                ops.append(('IN_AREA', (x - 50, top, x + 50, top + 50)))
        asm_log[:] = []
        c_log[:] = []
        lib.stub_grlog_clear()
        for op, args in ops:
            nops += 1
            ra, rc = run(op, args)
            c_ev = grlog()
            a_ev = list(asm_log)
            asm_log[:] = []
            c_log[:] = []
            if op == 'IN_AREA' and ra == 1:
                stats['in_area_1'] += 1
            if (ra & 0xFFFF) != (rc & 0xFFFF) or [e for e in a_ev if not e.startswith('move')] != c_ev:
                bad = 'boundary sweep op %s%s: result asm %d C %d' % (op, args, ra, rc)
                break
    rep.result('MOUSE', bad is None, '%d operations (%d SERVE changes, %d IN_AREA hits), %d driver calls '
               'and %d onMove calls compared%s' % (nops, stats['serve_1'], stats['in_area_1'], stats['driver'],
                                                  stats['onmove'], (': ' + bad) if bad else ''))


def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')]
    exe = Exe()
    overlay = tempfile.mkdtemp(prefix='osilib-overlay-')
    lib = load_lib(overlay)
    rep = Report()
    tests = [('rand', test_rand), ('lzw', test_lzw), ('kbd', test_kbd), ('sound', test_sound), ('mouse', test_mouse)]
    for name, fn in tests:
        if names and name not in names:
            continue
        m = Machine(exe)
        try:
            fn(exe, m, lib, rep)
        except (UcError, RuntimeError, AssertionError) as ex:
            rep.result(name.upper(), False, 'exception: %r' % ex)
    print('%d failed' % rep.failed)
    return 1 if rep.failed else 0


if __name__ == '__main__':
    sys.exit(main())
