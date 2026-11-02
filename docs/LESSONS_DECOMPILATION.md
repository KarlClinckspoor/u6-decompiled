# Lessons learned: decompiling Ultima VI byte for byte

This repository rebuilds the three programs of Ultima VI from source, identical to the
originals: `GAME.EXE` (ergonomy_joe's decompilation, made readable and then made exactly
identical), `U.EXE` (the menu, introduction and character creation) and `END.EXE` (the
ending). They are built with the original tools, Turbo C 2.0, MASM 3.0 and TLINK, in a
headless DOSBox, and compared byte for byte. This file collects what that work taught: what
mattered, what wasted time, and what to do differently next time. The how-to for each program
is in `SRC_U/DECOMP.md` and `SRC_END/DECOMP.md`; this is the why.

## 1. The ground rules that made it work

**Only the real compiler decides.** A decompilation that "looks right" or passes a
decompiler's round-trip is not the goal; the C is accepted when Turbo C 2.0 with the original
flags turns it into the original bytes. Everything else (a nicer name, a clearer expression)
comes after, and has to keep the bytes. Every change was checked by a rebuild, so the code
never drifted.

**Automate the whole loop first.** Before decompiling anything, three tools were built:

- a one-command compile in DOSBox (`tools/u/tcc.py`: copy the file in, run `tcc`, copy the
  object out; DOSBox runs headless with `SDL_VIDEODRIVER=dummy`);
- a one-module comparison (`tools/u/ucmp.py`): compile one C file, find its code in the
  original image at a given frame and offset, and report the *first instruction that really
  differs*, ignoring branch displacements and relocated fields, then check every fixup
  (calls, variables) against a symbol table and the string literals against the data
  segment;
- a full build and comparison (`tools/u/ubuild.py`), which reports the first differing bytes
  and the module they fall in.

With those, one attempt costs a few seconds and the answer is precise ("byte 0x1A4: `mov
ax,[bp-6]` vs `mov ax,si`"). Without them, each guess costs a manual build and a hex comparison.

**Work module by module, in link order, with known boundaries.** The linker lays code segments
end to end; a module's frame is its start rounded down to a paragraph. Once the boundaries
are known (from the disassembly: where one module's last `retf` ends and the next module's
first `push bp` starts, plus the frames used by far calls), each module can be decompiled and
checked on its own, and in parallel.

**Keep the address on everything.** Every function carries `/*C_SSSS_OOOO*/` and every
global its DS offset (`/*1E8C*/`). This survives renaming, lets anyone match the source to a
disassembly, and lets tools resolve names (`tools/u/usym.py`).

## 2. Turbo C 2.0: what it does, and the shapes it gives away

Most of the time goes into making Turbo C emit the same instructions. Its habits are
consistent, so they become recognisable:

- **Locals**: the first declared gets the lowest address. Register variables (`si`, `di`)
  go to the most-used int/pointer locals; declaration order breaks ties. If the original
  uses `si` for a variable you declared second, swap two declarations.
- **`a = b = c`** stores the rightmost first. A reload from the stack in the original means
  two statements, not one.
- **With `-O`**, identical tails of the branches of an `if` are merged (cross-jumping): a
  shared tail in the assembly does not mean shared code in the C.
- `cmp / jz / mov ax,1 / jmp / xor ax,ax` is `return a != b;`.
- `push cs / call near X` is a call to a function of the same module.
- A 2-byte argument cleanup is `inc sp / inc sp`; `pop cx` means more than 2 bytes or other
  flags.
- Struct assignment calls `SCOPY@`; an initialised local array is a `SCOPY@` from an
  anonymous initialiser in `_DATA`.
- Long arithmetic calls helpers (`LXLSH@`, `LXMUL@`, `LDIV@`, `LUDIV@`).
- `atoi` is a macro for `atol` when `<stdlib.h>` is included.
- Odd-looking code is often a literal C idiom. `while(!done && (done = y == -12, 1))` and
  `y += (-12 - y) * (y <= -12)` came out exactly as found in `END.EXE`: when the assembly is
  strange, try writing the strange C rather than "fixing" it.
- **Flags differ per library.** The game's C is `-mm -c -O -G` (cdecl for `U.EXE`/`END.EXE`;
  `GAME.EXE` is `-p`, pascal). Origin's OSI library C was compiled *without* `-O`. A module
  that will not match with any C may just have different flags.

## 3. The data segment is half the work

Matching the code is not enough: data must sit at the same offsets, because the code
contains them.

- **Initialised data** is laid out per module in link order, variables in definition order,
  then the module's string literals in source order. So the order of definitions in the C,
  and where each literal first appears, matter. `ucmp --data-range` compares a module's
  whole `_DATA`.
- **Uninitialised data (`_BSS`) cannot be written in C.** Turbo C orders a module's `_BSS`
  by its symbol table (a hash of the names), so `D_6080` lands before `D_6072` and no
  declaration order fixes it. The C declares those variables `extern`, and a `BSS.ASM`
  lays them out with `LABEL` and `ORG $+n` (not `dup(?)`: MASM 3.0 writes data records for
  it, which would put the zeros into the `.EXE`).
- Gaps in the data map are real. Something occupied them; find out what before assuming
  padding.

## 4. Assembly: TASM vs MASM

Origin assembled its hand-written assembly with TASM; the toolchain here has MASM 3.0. They
mostly produce the same bytes, except:

- **Instruction encodings.** TASM encodes `AND r/m, imm` with a small constant in the
  sign-extended form (`83 /4`); MASM 3.0 always uses `81 /4`. Six such instructions in
  `GAME.EXE`'s `OSI_FILE.ASM` and `MOUSE.ASM` were the last 6 bytes keeping it from being
  identical. They are now `db` with the instruction in a comment.
- **Offsets and jumps.** `OFFSET sym` is segment-relative in MASM: write
  `OFFSET DGROUP:sym`. A forward far call needs `FAR PTR`; a forward `JMP` is near unless
  `SHORT`.
- **Externals declared late.** Turbo C writes the `extrn` of a variable used in inline
  assembly at the *end* of its `.ASM` output. MASM then resolves `mov es,egaPage`
  relative to `_BSS` instead of `DGROUP`, off by the size of `_DATA`. Writing
  `asm mov es,DGROUP:egaPage` fixed the last 2 bytes of `END.EXE`.
- **Relocation order.** TASM writes some fixups of a module in a different order than the
  code; the `.EXE` relocation table follows that order. `tools/u/tasmorder.py` reorders the
  fixups of an object file as a `;TASM fixup order:` comment in the source says. Without
  it, the image matched but the header did not.
- **Inline assembly.** C files with `asm` statements were compiled through the assembler
  (`tcc -B`); `tcc -S` followed by MASM 3.0 `/Mx` gives the same object.
- **Odd tricks are the original's.** `END.EXE`'s star routines read their variables
  through `CS:` (the linker makes the offsets relative to the code segment, which reaches the
  same memory), and they pop more words than they push, returning straight into the timer
  interrupt handler past the caller's `popf`. Such code is kept as it is and documented;
  the tools learned to accept it.

## 5. The linker and the libraries

- **Library module order is layout.** TLINK places library modules in the order they appear
  in the library. Origin's `CM.LIB` is Turbo C's with `clreol()` and `gotoxy()` replaced by
  OSI versions, and to get the same layout the library is rebuilt with the replacements in
  Turbo C's places (`ubuild.py`). One module (`CLREOL`) had to be an empty segment start,
  with its code living in another module, because that was the only way to get both the
  code position and the relocation position right.
- **Identify library code before decompiling it.** `END.EXE` links the same OSI library
  objects as `U.EXE` and the same CM.LIB modules; matching the image against the existing
  objects (`libmatch.py` style: compare bytes with relocations masked) quickly separated the
  program's own code from the libraries and left only three modules to decompile.
- **Library calling conventions are not uniform.** `END.EXE` calls `PAL_load` as a cdecl C
  function where the library exports it as an assembly name without the underscore. The
  library got an alias label at the same address; `U.EXE` is unchanged.

## 6. Tools must not lie

Some of the differences reported were not differences at all:

- the comparison tool took the offset fixup of a far function pointer (`TIM_setCallback(EndTick)`)
  as a data reference, then complained that the segment was not DGROUP;
- it flagged `CS:`-relative offsets as wrong;
- it treated switch jump tables as code when looking for the first difference.

Each time the fix was in the tool, not the C. When a report looks impossible, check the tool
first, and fix it so the next module does not hit the same thing. Equally, a tool that
reports "0 problems" is only as good as what it checks: the final word is always the full
build compared byte for byte, header and relocation table included.

## 7. Working with parallel agents

`U.EXE` and `END.EXE` were decompiled largely by AI agents working in parallel, one per module
or group of modules. What worked:

- **A written brief per program** (`END.EXE`'s facts: DGROUP, the module frames and lengths,
  where the libraries start, which tools to use, what "done" means: `ucmp` reports 0
  differences and 0 problems).
- **Small modules first, to a separate agent**, while one agent took the large main module.
  The small ones finished first and validated the brief.
- **Shared headers with an owner.** Two agents writing the same header (`end.h`) is fine if
  one creates it and the other only adds; otherwise names collide.
- **Agents may not change the shared tools**, but they report tool problems precisely. The
  `EndTick` false report came back with the exact cause and the suggested fix.
- **Commit work in progress often.** Long agent runs end, sessions end; a pushed WIP commit
  loses nothing.

## 8. Readability without changing the bytes

`GAME.EXE`'s readability pass (about 300 functions and 400 globals renamed, every function
commented, files renamed by content) was done before the toolchain was available here, so
it was checked differently: `tools/verify.py` preprocesses each translation unit before and
after and compares the token streams, allowing only renames from a map, consistent local
renames, and number spellings with the same value and type. Codegen depends only on the
token stream, so this is a proof, but the real rebuild later confirmed it.

Rules that kept it safe: do not change control flow, expression shape, declaration order,
data, or literals; rename assembly symbols only where C shares them; keep the old address in
a comment. Bugs found along the way go into `FINDINGS.md`, never into the code.

## 9. What we would do the same way again

- Build the compile-and-compare loop before writing any C.
- Name things as soon as their purpose is clear, with the address kept in a comment.
- Lay out `_BSS` in assembly from the start instead of fighting Turbo C's ordering.
- Treat every "impossible" difference as a tool bug until shown otherwise.
- Keep each program's quirks in its own `DECOMP.md` (`SRC_U/DECOMP.md`, `SRC_END/DECOMP.md`),
  written while the knowledge is fresh.

## 10. What we would do differently

- Find the library modules (by matching existing objects) before cutting the program into
  modules; it saves decompiling code that already exists.
- Check the assembler's encoding choices (TASM's short forms) on day one with a tiny test,
  rather than discovering them as the last 6 bytes.
- Give the comparison tool a "learned symbols" review step; one wrong learned name (a
  function taken for data) costs more than the time it saves.
- Keep generated helper files (`endsyms.txt`, `usyms.txt`) under version control from the
  start, so that every machine checks the same thing.
