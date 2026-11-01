# END.EXE: Ultima VI's endgame, decompiled

GAME.EXE runs END.EXE when the Codex's lenses are placed:
`end.exe <config> <years> <months> <days>`. It plays the ending: Lord British and King
Draxinusom raise the two lenses, the light goes through them onto the Codex, the text
of the ending (sixteen scenes), and "CONGRATULATIONS ... in N years, M months, and D
days" under the stars.

These sources build END.EXE byte for byte (40112 bytes, sha1
`91868fa6b5e8f5c2a85d2b5393a74ab2ae268005`) with Turbo C 2.0, MASM 3.0 and TLINK:

    U6_PROG=end python3 tools/u/ubuild.py     # build and compare with END.EXE
    U6_PROG=end python3 tools/u/ucheck.py     # each module against the image

END.EXE comes from the same team and the same libraries as U.EXE, so everything in
SRC_U/DECOMP.md (the compiler flags, the layout, what Turbo C does) applies here, and the
build shares SRC_U's files: the OSI library (SRC_U/OSILIB, END.EXE links the same
objects), Origin's CM.LIB, `lzw.asm` (LoadZFile), and `u.h`.

| module | frame:at | length | what |
|---|---|---|---|
| main.c | 00A9:4 | 204D | the configuration, the drivers, the scenes, the text, the music (much as U.EXE's) |
| tick.c | 02AE:1 | 13B | the timer callback (the stars twinkle, the colours cycle), the driver's segment, Hercules (inline asm) |
| stars.asm | 02C1:C | 46D | the stars for MCGA, EGA, CGA and Tandy |
| lzw.asm | 0308:A | 365 | SRC_U/lzw.asm |
| BSS.ASM | | | the uninitialised data of main.c and tick.c (DS 1770..2114) |

Link order: `SRC_END/end.lnk`.

Notes
- The data the modules share is declared in `end.h`; the rest of the _BSS in main.c.
- main.c calls `PAL_load` as a C function, so the OSI library's PALETTE.ASM also exports
  it as `_PAL_load` (a label at the same address: U.EXE is unchanged).
- END.EXE's `TIM_setCallback` takes a far function pointer.
- tick.c's `asm mov es,DGROUP:egaPage`: MASM resolves the `extrn` Turbo C writes at the end
  of the file relative to _BSS, not to DGROUP, without the override.
- stars.asm reads its variables through `CS:` (the offsets are relative to its own
  frame, which reaches the same memory as DGROUP's). Its routines leave the stack
  unbalanced: they return through the timer interrupt handler, past EndTick's `popf`.
