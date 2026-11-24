# What is not decompiled yet

GAME.EXE, U.EXE and END.EXE rebuild byte for byte (README, "Verified with the real compiler").
The other programs of the GOG release have no sources. This is the survey and effort estimate
made on 2026-11-23 (nothing below was decompiled). "Sessions" are agent work sessions; the
estimates are loose (about +/-50%).

## Findings

- **Drivers** are LZW-compressed flat binaries starting with a jump table: position
  independent, no relocations, hand-written assembly. Many sign-extended AND encodings (`83 /4`)
  point to TASM, so MASM 3.0 would need a few `db` lines, as in `OSILIB/OSI_FILE.ASM` and
  `MOUSE.ASM`.
- **Compression is solved:** `tools/lzwpack.py` (a plain greedy LZW, reset when 4096 codes are
  full) recompresses all fifteen compressed driver files byte for byte
  (`python3 tools/lzwpack.py --check gamedata/U6*.DRV gamedata/*DRV.BIN`).
- **Utilities** are Turbo C 2.0, medium model, TLINK (the toolchain already used here). Each
  contains a copy of the OSI LZ.INC decompressor; INSTALL and the T2* programs share a ~1 KB
  disk-prompt module. MAKEMODE, T2EGA and T2TGA are EXEPACKed (`tools/exepack.py` unpacks them).
- **ULTIMA6.EXE** is a small Microsoft C program (`tandy.c`, small model, SLIBCE.LIB, CodeView
  NB00 debug information kept in the file). Its run-time library says "Copyright (c) 1989"; MSC
  5.1's says 1988, so the compiler is probably QuickC 2.0/2.01. The exact compiler and LINK are
  needed, and the CodeView block must match too.

| Program | File -> unpacked | Packing | Toolchain (likely) | Own code | Already reverse-engineered | Similar to | Sessions |
|---|---|---|---|---|---|---|---|
| TANDYPIN.EXE | 624 -> 112 | none | MASM + MS LINK | 13 bytes (a TSR) | - | - | < 0.25 |
| U6ADLIB.DRV | 1217 -> 1662 | LZW | asm (TASM?) | 4 entries | port: adlib.c + diff test | U6INNOVA 30% | 0.5 |
| U6INNOVA.DRV | 1190 -> 1611 | LZW | asm | 4 entries | - | U6ADLIB 30% | 0.5 |
| U6CMS / U6COVOX / U6TMUS | 1455 -> 1986, 1436 -> 1964, 1272 -> 1775 | LZW | asm | 4 entries each | - | each other 43-70% | 0.5 + 0.3 + 0.3 |
| U6ROLAND.DRV | 1058 -> 1465 | LZW | asm | 5 entries | - | <= 7% | 0.5 |
| MCGADRV.BIN | 1940 -> 2632 | LZW | asm | 9 entries | docs/port/U_DRIVER.md, port/u/ugfx.c | - | 0.5 |
| TGADRV.BIN / CGADRV.BIN | 2379 -> 2873, 3296 -> 4218 | LZW | asm | 10 entries each | interface only | each other 31-41% | 1 + 1 |
| EGADRV.BIN / EEGADRV.BIN | 3650 -> 4922, 3623 -> 4870 | LZW | asm | 11 entries each | interface only | each other 73% | 1.5 + 0.5 |
| U6MCGA.DRV | 4961 -> 13796 | LZW | asm | 27 entries | full: MCGA_DRIVER.md, mcga.c, diff test | - | 1-1.5 |
| U6TANDY.DRV | 5930 -> 14795 | LZW | asm | 27 entries | interface | MCGA 24-28% | 2 |
| U6CGA.DRV | 7301 -> 16676 | LZW | asm | 27 entries | interface | TANDY 22-28% | 2-3 |
| U6EGA.DRV | 6847 -> 14744 | LZW | asm | 27 entries, planar | interface | <= 19% | 2-3 |
| ULTIMA6.EXE | 18677 (11295 image + 6870 CodeView) | none | MS C 1989 (QuickC 2.x?), LINK /CO | ~470 bytes | - | - | <= 0.5 once the compiler is found |
| INSTALL.EXE | 27250 | none | TC 2.0 -mm, TLINK | ~7.5 KB, ~18 functions | - | shares a module with T2* | 1.5-2 |
| T2CGA.EXE + T2HRC.EXE | 23026 each | none | TC 2.0 | ~6 KB, ~15 functions | - | each other: 20 bytes differ | 1.5 + 0.1 |
| T2TGA.EXE | 21453 -> 21920 | EXEPACK | TC 2.0 | ~6.4 KB | - | T2CGA | 1 |
| T2EGA.EXE | 22747 -> 23136 | EXEPACK | TC 2.0 | ~7.3 KB | - | - | 1-1.5 |
| MAKEMODE.EXE | 24201 -> 24336 | EXEPACK | TC 2.0 | ~11 KB | - | - | 1.5-2 |

Totals: drivers about 15 sessions, utilities about 7.5, ULTIMA6 + TANDYPIN about 1 (plus finding
the compiler), and about 0.5 for a driver build script (assemble, link, strip the header, LZW,
compare).

## Suggested order

1. TANDYPIN.EXE: trivial, and proves the MS LINK header.
2. The music drivers: U6ADLIB (there is a spec), U6INNOVA, U6TMUS/U6COVOX/U6CMS, U6ROLAND. Small,
   and they set up the driver build pipeline.
3. U6MCGA.DRV and MCGADRV.BIN, already documented.
4. EGADRV/EEGADRV together, then TGADRV/CGADRV.
5. U6TANDY, U6CGA, U6EGA: the largest, not reverse-engineered yet.
6. The Turbo C utilities at any time, in parallel: INSTALL, T2CGA (then T2HRC), T2TGA, T2EGA,
   MAKEMODE.
7. ULTIMA6.EXE when the exact Microsoft compiler and LINK are at hand.

Open: whether MASM 3.0's 1984 LINK reproduces TANDYPIN's header; whether the utilities use stock
or modified CM.LIB modules.
