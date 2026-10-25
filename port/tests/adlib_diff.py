#!/usr/bin/env python3
"""Differential test: port/sys/adlib.c against the original U6ADLIB.DRV.

The original driver (unpacked from U6ADLIB.DRV in the game folder) runs in unicorn; its
OPL writes (OUT 388h index, OUT 389h data) are recorded, its timer interrupt handler is
called as the PIT would. The C driver (built with port/tests/adlib_shim.c into a shared
library) records the writes it makes to the emulated chip. Every song (*.M) plays for
a few thousand ticks on both, and the writes of each call and each tick must be the same.

  python3 port/tests/adlib_diff.py [--ticks N]

Exit status 0 when everything matched.
"""
import argparse, ctypes, os, subprocess, sys, tempfile

from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_INSN, UC_HOOK_INTR
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BX, UC_X86_REG_SP, UC_X86_REG_CS,
                               UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS, UC_X86_REG_IP,
                               UC_X86_INS_IN, UC_X86_INS_OUT)

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.dirname(HERE)
GAMEDATA = os.environ.get('U6_GAMEDATA', '/home/claude/gamedata')
sys.path.insert(0, HERE)
import u6lzw  # noqa: E402

DRVSEG, SONGSEG, STUB = 0x2000, 0x3000, 0xF000
RET, IRET_TO, OLD8 = 0x0100, 0x0200, 0x0300


def gamefile(name):
    for f in os.listdir(GAMEDATA):
        if f.upper() == name.upper():
            return open(os.path.join(GAMEDATA, f), 'rb').read()
    raise FileNotFoundError(name)


class Orig:
    def __init__(self, image):
        self.mu = mu = Uc(UC_ARCH_X86, UC_MODE_16)
        mu.mem_map(0, 0x110000)
        mu.mem_write(DRVSEG * 16, image)
        mu.mem_write(STUB * 16 + RET, b'\xf4')
        self.log, self.index, self.pit = [], 0, 0xffff
        mu.hook_add(UC_HOOK_INSN, self.on_out, None, 1, 0, UC_X86_INS_OUT)
        mu.hook_add(UC_HOOK_INSN, self.on_in, None, 1, 0, UC_X86_INS_IN)
        mu.hook_add(UC_HOOK_INTR, self.on_int)

    def on_out(self, mu, port, size, value, ud):
        if port == 0x388:
            self.index = value & 0xff
        elif port == 0x389:
            self.log.append(self.index << 8 | (value & 0xff))

    def on_in(self, mu, port, size, ud):
        if port == 0x40:            # the PIT's counter, read twice a sample: counts down
            self.pit = (self.pit - 23) & 0xffff
            return self.pit & 0xff if self.pit & 1 else self.pit >> 8
        return 0

    def on_int(self, mu, intno, ud):
        if intno == 0x21:
            ah = mu.reg_read(UC_X86_REG_AX) >> 8
            if ah == 0x35:          # get vector: the "old" handler is a stop address
                mu.reg_write(UC_X86_REG_ES, STUB)
                mu.reg_write(UC_X86_REG_BX, OLD8)
            return
        raise RuntimeError('int %02x' % intno)

    def _run(self, ip, stops):
        mu = self.mu
        mu.reg_write(UC_X86_REG_CS, DRVSEG)
        mu.reg_write(UC_X86_REG_IP, ip)
        start = DRVSEG * 16 + ip
        while True:
            try:
                mu.emu_start(start, 0, count=2000000)
            except Exception:
                pass
            lin = mu.reg_read(UC_X86_REG_CS) * 16 + mu.reg_read(UC_X86_REG_IP)
            if lin in stops or lin - 1 in stops:
                return
            raise RuntimeError('stopped at %05x' % lin)

    def call(self, entry, args=()):
        mu = self.mu
        mu.reg_write(UC_X86_REG_SS, 0x1000)
        mu.reg_write(UC_X86_REG_DS, 0x1000)
        sp = 0xfff0
        for a in args:              # pascal: left to right
            sp -= 2
            mu.mem_write(0x10000 + sp, bytes([a & 0xff, a >> 8 & 0xff]))
        sp -= 4
        mu.mem_write(0x10000 + sp, bytes([RET & 0xff, RET >> 8, STUB & 0xff, STUB >> 8]))
        mu.reg_write(UC_X86_REG_SP, sp)
        self._run(entry, {STUB * 16 + RET})

    def tick(self):
        mu = self.mu
        mu.reg_write(UC_X86_REG_SS, 0x1000)
        sp = 0xfff0 - 6
        mu.mem_write(0x10000 + sp, bytes([IRET_TO & 0xff, IRET_TO >> 8, STUB & 0xff, STUB >> 8, 0x02, 0x02]))
        mu.reg_write(UC_X86_REG_SP, sp)
        mu.mem_write(STUB * 16 + IRET_TO, b'\xf4')
        mu.mem_write(STUB * 16 + OLD8, b'\xf4')
        self._run(0x3a0, {STUB * 16 + IRET_TO, STUB * 16 + OLD8})

    def take(self):
        l, self.log = self.log, []
        return l


class Port:
    def __init__(self, lib):
        self.l = ctypes.CDLL(lib)
        self.l.t_init()
        self.l.t_arena.restype = ctypes.POINTER(ctypes.c_uint8)
        self.arena = self.l.t_arena()
        self.buf = (ctypes.c_uint16 * 65536)()
        self.l.t_state.restype = ctypes.POINTER(ctypes.c_uint8 * 0x400)
        self.state = self.l.t_state().contents

    def take(self):
        n = self.l.t_log(self.buf, 65536)
        return list(self.buf[:n])


def build(tmp):
    out = os.path.join(tmp, 'libadlib_test.so')
    srcs = [os.path.join(PORT, 'sys', 'adlib.c'), os.path.join(PORT, 'sys', 'dosmem.c'),
            os.path.join(HERE, 'adlib_shim.c')]
    subprocess.check_call([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-O1', '-shared',
                           '-fPIC', '-o', out] + srcs)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ticks', type=int, default=6000)
    a = ap.parse_args()
    image = u6lzw.decompress(gamefile("U6ADLIB.DRV"))[0]
    songs = sorted(f for f in os.listdir(GAMEDATA) if f.upper().endswith('.M'))
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        lib = build(tmp)
        for name in songs:
            song = u6lzw.decompress(gamefile(name))[0]
            o, p = Orig(image), Port(lib)
            o.mu.mem_write(SONGSEG * 16, song)
            ctypes.memmove(ctypes.addressof(p.arena.contents) + SONGSEG * 16, song, len(song))
            o.call(0, [0x388])
            p.l.t_drv0(0x388)
            p.l.t_prime()
            p.take()
            o.take()
            steps = [('play', lambda: o.call(6, [SONGSEG, 0]), lambda: p.l.t_drv6(SONGSEG, 0))]
            steps += [('tick %d' % i, o.tick, p.l.t_tick) for i in range(a.ticks)]
            steps += [('stop', lambda: o.call(9), p.l.t_drv9)]
            ok = True
            writes = 0
            for what, fo, fp in steps:
                fo()
                fp()
                lo, lp = o.take(), p.take()
                so = bytes(o.mu.mem_read(DRVSEG * 16 + 0x0c, 0x13a - 0x0c))
                sp_ = bytes(p.state[0x0c:0x13a])
                if so != sp_ and ok:
                    k = next(i for i in range(len(so)) if so[i] != sp_[i]) + 0x0c
                    print('  all:', ' '.join('%03x:%02x/%02x' % (i + 0x0c, so[i], sp_[i]) for i in range(len(so)) if so[i] != sp_[i]))
                    print('%s: %s: the variables differ first at %03x: original %s port %s' % (
                        name, what, k, so[k - 0x0c:k - 0x0c + 8].hex(), sp_[k - 0x0c:k - 0x0c + 8].hex()))
                    ok = False
                writes += len(lo)
                if lo != lp:
                    print('%s: %s differs:\n  original %s\n  port     %s' % (
                        name, what, ' '.join('%04x' % x for x in lo[:40]), ' '.join('%04x' % x for x in lp[:40])))
                    ok = False
                    break
            print('%-12s %s (%d writes)' % (name, 'same' if ok else 'DIFFERENT', writes))
            bad += not ok
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
