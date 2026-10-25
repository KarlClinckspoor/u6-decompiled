# U.EXE decompiled

`U.EXE` is the program `ULTIMA6.EXE` starts: the main menu, the introduction, the gypsy
and character creation, the transfer of a character from Ultima IV or V, the credits.
`ULTIMA6.EXE` (a small Microsoft C launcher) then starts `GAME.EXE`. This folder is its
decompilation: C and assembly that Turbo C 2.0, MASM 3.0 and TLINK build back into the
same `U.EXE`, byte for byte (header and relocation table included), the way `SRC/` does
for `GAME.EXE`.

    python3 tools/u/ubuild.py [--keep DIR]    build U.EXE in DOSBox and compare it
    python3 tools/u/ucheck.py [MODULE ...]    compare each module with U.EXE

Both need DOSBox and the DOS tools (Turbo C 2.0, MASM 3.0) that `tools/dosbuild.py`
uses (`U6_DOSTOOLS`), and the original U.EXE to compare with (`U6_UEXE`, or
`gamedata/U.EXE`).

## The program

| frame | module | what |
|---|---|---|
| 00E1 | main.c | configuration, keyboard, palettes, the main menu, `main()`, music |
| 0263 | transfer.c | the character sheet; a character from Ultima IV's party.sav or V's saved.gam |
| 0406 | chargen.c | the gypsy: the questions, the vials, the pattern; the name and the portrait |
| 0676 | credits.c | the credits |
| 06E9 | intro.c | the introduction: the monitor, the clouds and rain, the storm, the scripts |
| 09A0 | passage.c | the text passages of the introduction |
| 0BC1 | disk.c | the game disks: which drive has which disk, "insert disk" |
| 0C17 | moongate.c | the moongate, the stars twinkling, the hands, starting GAME.EXE |
| 0F15 | criterr.asm | the critical error (int 24h) handler |
| 0F18 | lzw.asm | `LoadZFile`: load an LZW-compressed driver (MCGADRV.BIN, U6ADLIB.DRV, ...) |
| | BSS.ASM | the uninitialised data of all the modules (see below) |
| 1277.. | OSILIB/ | Origin's OSI library: surfaces, text windows, the mouse, files (pascal), fades, dissolves, LZW, the timer |

All the C is `tcc -mm -c -O -G`: medium model (far code, near data), **cdecl** (GAME.EXE
is `-p`). The OSI library's C is `-mm -c -G` (no `-O`). Every function has its address
before it (`/*C_SSSS_OOOO*/`: frame, offset) and every global its DS offset
(`/*XXXX*/` in C, `;D_XXXX` in assembly); `tools/u/usym.py list` lists them all.

## How it is put together

TLINK packs the code segments without alignment: a module starts where the previous one
ends, and its frame (the segment of its far addresses) is its start rounded down to a
paragraph. `ucmp.py` shows module-relative offsets (frame offset minus the module's
start, `--at`; `ucheck.py` has each module's frame, start, length and data range).

The link: `TLINK C0M main transfer chargen credits intro passage disk moongate criterr
lzw bss, U.EXE, , CM OSILIB` (case-insensitive, like Origin's), with:

- **CM.LIB, Origin's**: Turbo C's, with `clreol()` and `gotoxy()` replaced by versions
  that work on the OSI text window. TLINK lays out library modules in the order of the
  library, so `ubuild.py` rebuilds CM.LIB with the replacements in Turbo C's places
  (`OSILIB/CLREOL.ASM`, `OSILIB/GOTOXY.ASM`). `clreol()`'s code is in `OSILIB/TXTCLR.ASM`,
  and CM.LIB's CLREOL module only names the start of its segment: that is the only way
  for the code to be where Turbo C's is and for its relocation to be in the table among
  TXTCLR's (see CLREOL.ASM).
- **OSILIB.LIB**, in the order of `OSILIB/osilib.lst`.

The data segment (DGROUP 1499): C0M's data (0000-0194), each module's `_DATA` in link
order (its initialised variables in the order of definition, then its string literals),
the libraries' data; then the uninitialised data: the modules' (4A14-66E4), CM.LIB's
(66E4-681B), the OSI library's (681C-683E). Turbo C orders a module's `_BSS` by its
symbol table (by a hash of the names), so the C declares those variables `extern`, and
`BSS.ASM` (`OSILIB/LIBBSS.ASM` for FADE and TIMER) lays them out with `LABEL`s and
`ORG`s (MASM 3.0 writes data records for `dup(?)`).

The assembly: Origin used TASM. MASM 3.0 makes the same bytes, with these differences
taken care of:

- `OFFSET sym` is relative to the segment: `OFFSET DGROUP:sym` where the original has a
  DGROUP offset. A forward far call needs `FAR PTR`; a forward `JMP` is near unless
  `SHORT` (`JMPN`, `JMPF` in OSI.INC for the cases TASM made differently).
- TASM wrote the fixups of some data records in another order than the code's, and the
  .EXE relocation table follows that order: `tools/u/tasmorder.py` rearranges the
  object's fixups as the module's `;TASM fixup order:` comment says (six modules).
- C modules with inline assembly (main.c; FADE.C, TIMER.C, SURFSHR.C in the library)
  were compiled through the assembler (`tcc -B`): `tcc -S` then MASM 3.0 `/Mx` gives
  the same code (`#pragma inline` marks the library ones).

## What Turbo C does

- Locals: the first declared gets the lowest address (furthest from bp). Register
  variables (si, di) go to the most used int/pointer locals; the order of declaration
  decides between equals.
- `a = b = c` stores the rightmost first; a reload from the stack in the original means
  two statements.
- With `-O`, identical tails of the branches of an `if` are merged (cross-jumping).
- `cmp/jz/mov ax,1/jmp/xor ax,ax` is `return a != b;`.
- `push cs / call near X` is a call to a function of the same module.
- A two-byte argument cleanup is `inc sp / inc sp`; `pop cx` means other flags.
- Struct assignment calls `SCOPY@` (cx = size); an initialised local array is a
  `SCOPY@` from an anonymous initialiser in `_DATA`.
- Long shifts call `LXLSH@`; long multiply `LXMUL@`; long divide `LDIV@`/`LUDIV@`.
- `atoi` is a macro for `atol` (with `<stdlib.h>`).
- The graphics driver call `DRV(o)(args)` (`u.h`) is two statements: brace it in an `if`.

## Tools

    python3 tools/u/udis.py SEG [END]         annotated disassembly of a module
    python3 tools/u/ucmp.py FILE --seg SEG --at AT --include SRC_U --length LEN
                                              compile one module and compare it
    python3 tools/u/tcc.py [--masm] FILE      just compile (assemble) in DOSBox
    python3 tools/u/usym.py list|apply        the symbol table; turn address names
                                              (C_SSSS_OOOO, D_XXXX) into names

`ucmp.py` reports the first instruction that really differs (branch displacements and
fixups aside), checks every fixup against the original (calls and variables by name or
address), and compares the string literals with the data segment.
