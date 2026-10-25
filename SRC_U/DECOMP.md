# Decompiling U.EXE

`U.EXE` is the program `ULTIMA6.EXE` starts: the main menu, the introduction, the gypsy
and character creation, the transfer of a character. `ULTIMA6.EXE` (a small Microsoft C
launcher) then starts `GAME.EXE`. This folder is its decompilation: C that Turbo C 2.0
compiles back to the same bytes, the way `SRC/` does for `GAME.EXE`.

## The binary

Turbo C 2.0, medium model (far code, near data), **cdecl** (not `-p` like GAME.EXE).
Compiler flags: `tcc -mm -c -O -G` (checked on segment 0BC1).

| image | segment | what |
|---|---|---|
| 00000-009F0 | 0000 (_TEXT) | C0M start-up and Turbo C helpers (SCOPY@, LXLSH@, ...) |
| 009F0-00E10 | 0000 (_TEXT) | OSI file routines in assembly (OSI_open 09F3, OSI_close 0A32, OSI_read 0AE4, OSI_write 0BCA: **pascal**) |
| 00E10-0F4F0 | 00E1 0263 0406 0676 06E9 09A0 0BC1 0C17 0F15 0F18 | the C modules, in link order |
| 0F4F0-1277C | 0F4F..1276 | Turbo C 2.0 CM.LIB modules (named in tools/u/usyms.txt) |
| 1277C-14990 | 1277..1482 | the OSI library: assembly (and maybe C), cdecl, like GAME.EXE's SRC/OSILIB |
| 14990- | 1499 | DGROUP: C0M's data (0000-0194), then each module's _DATA in link order (its initialised variables, then its string literals), then the libraries' data; uninitialised data (_BSS) from about 4A14 |

TLINK packs the modules' code segments without alignment: a module starts where the
previous one ends, and its frame (the segment of its far addresses) is its start rounded
down to a paragraph. The address names C_SSSS_OOOO are frame-relative; `ucmp.py` shows
module-relative offsets (frame offset minus the module's start offset `--at`).

| frame | image | --at | length | 
|---|---|---|---|
| 00E1 | 00E16-02630 | 6 | 181A |
| 0263 | 02630-04066 | 0 | 1A36 |
| 0406 | 04066-06766 | 6 | 2700 |
| 0676 | 06766-06E9E | 6 | 738 |
| 06E9 | 06E9E-09A0B | E | 2B6D |
| 09A0 | 09A0B-0BC10 | B | 2205 |
| 0BC1 | 0BC10-0C170 | 0 | 560 (disk.c, done) |
| 0C17 | 0C170-0F156 | 0 | 2FE6 |
| 0F15 | 0F156-0F18E | 6 | 38 |
| 0F18 | 0F18E-0F4F3 | E | 365 (ends with an iret: an interrupt function) |

## Conventions

- A function is preceded by its address comment, as in SRC/: `/*C_0BC1_0031*/` (segment,
  offset). Give the functions you define descriptive names.
- Anything **defined in another module** is referred to by its address name, declared
  `extern` at the top of your file with the best type you can tell:
  `C_SSSS_OOOO()` for functions, `D_XXXX` for data (DS offset, upper-case hex). A final
  pass replaces these with the names their modules gave them.
- For now **every global variable is `extern D_XXXX`**, even those that live in your
  module's _DATA/_BSS: the data layout is done in a second pass. Only string literals
  stay inline (they are your module's _DATA, after its variables). A comment after the
  declaration saying what the variable is helps the final pass name it.
- Library functions by their Turbo C names, with the Turbo C headers (`<dos.h>`,
  `<alloc.h>`, `<conio.h>`, `<string.h>`, `<stdio.h>`, `<process.h>`, ...): prototypes
  change the code, so include what the original code needs. `u.h` has the OSI file
  routines (pascal) and the graphics driver call `DRV(o)(args)` (a far pointer at 4A34
  whose offset selects the entry, like GAME.EXE's `MUS_DRV`).
- Code style: tabs, `if(`, `for(`, the comments explain what the code does and why, as
  in SRC/.

## What Turbo C does (found so far)

- Locals: the first declared gets the lowest address (furthest from bp). Register
  variables (si, di) are given to int/pointer locals automatically; try the order of
  declaration when si/di come out swapped.
- `x = y = 0` stores y first. `a = b = c` vs two statements makes a difference (a
  reload from the stack in the original means two statements).
- `cmp/jz/mov ax,1/jmp/xor ax,ax` is `return a != b;`.
- `push cs / call near X` is a call to a function of the same module.
- A two-byte argument cleanup is `inc sp / inc sp`; `pop cx` means other flags.
- Struct assignment calls `SCOPY@` (cx = size).
- Long shifts call `LXLSH@`; long multiply `LXMUL@`; long divide `LDIV@`/`LUDIV@`.

## Tools

    python3 tools/u/udis.py SEG [END]         annotated disassembly of a module
    python3 tools/u/ucmp.py FILE.c --seg SEG --at AT --include SRC_U --length LEN
                                              compile and compare with U.EXE
    python3 tools/u/tcc.py [--masm] FILE      just compile (assemble) in DOSBox

`ucmp.py` reports the first instruction that really differs (branch displacements and
fixups aside), checks every fixup against the original (calls by name or address,
DS offsets), and compares the string literals with the data segment. A module is done
when it says `0 bytes differ`, `data ... same`, `0 problems`, with `--length` the
module's length from the table above.

`disk.c` (frame 0BC1) is a finished example. GAME.EXE's decompilation in `SRC/` comes
from the same team and libraries: grep it for similar code (driver calls, console,
mouse, file handling) and for names.
