# Lessons learned: porting Ultima VI to Windows and Linux

The native port (`port/`, see `docs/port/PORT.md`) compiles the game's own decompiled C,
the same files that still build byte-identical DOS programs, for 64-bit Windows and Linux on
SDL2. It runs all three programs of the game, `U.EXE` (menu, introduction, character
creation), `GAME.EXE` and `END.EXE` (the ending), in one executable, with the AdLib music, a
settings screen (F11), and a recording of every session that replays exactly on any
platform. This file collects what the port taught.

## 1. One source, two targets

**The shared sources stay the DOS sources.** The decision that shaped everything: no fork.
`SRC/`, `SRC_U/` and `SRC_END/` compile with Turbo C 2.0 to the original bytes *and* with
clang/gcc/mingw for the host. Every change to them is checked by a DOS rebuild. Two
mechanisms make that possible:

- **`portable.h` macros that expand to the original tokens under Turbo C.** `int16`,
  `uint16`, `int32`, `uint32` are `int`, `unsigned`, `long`, `unsigned long` for Turbo C and
  the `stdint` types on the host. The same goes for the varargs of `CON_printf`, a pointer
  advanced through a cast lvalue, near pointers kept in integers, and a string literal the
  game writes into. If the macro's Turbo C expansion is token-for-token the original,
  the DOS build cannot change.
- **A force-included compatibility header** (`port/include/compat.h`) that gives the host
  the meaning of DOS: `far`/`near` vanish, `MK_FP`/`FP_SEG` work on a 1 MB DOS arena
  (`port/sys/dosmem.c`), Borland's library names exist, every struct is packed.

Changes in the shared C that only the host needs go behind `#ifndef __TURBOC__` (or
`#ifdef __TURBOC__` around inline assembly). Then the DOS build is re-run.

## 2. A 16-bit program on a 64-bit machine

Most crashes and most replay divergences came from the same root: the game assumes Turbo C's
16-bit world.

- **Integer widths.** `int` is 16 bits in the game. All of it was rewritten with explicit
  widths first (`portable.h`), and `-fwrapv` keeps overflow wrapping as the 8086 did. Where
  the game calls a function without a prototype, the host passes a 32-bit `int` and
  nothing narrows it: the port generates prototypes for every function
  (`port/tools/gen_u.py`, `port/include/portprot.h`), and library routines that take `int`
  cut it to 16 bits inside.
- **The memory layout is part of the program.** The game reads past the end of arrays into
  the next variable (combat reads `MonsterFlags[-1]`, which in DOS is the end of
  `MonsterAlignment`), and writes over neighbours. So the uninitialised data is one block
  in DOS's order (`gen_bss.py`, `gen_u.py`), and the blocks the game abuses (the saved-game
  state, the monster tables) are laid out exactly as DOS had them (`gen_block.py`). Objects
  with pointers grow on the host; the generator keeps everything else contiguous.
- **Alignment.** Those variables sit at odd DOS offsets inside the blocks, but clang assumes
  a global array of 16 bytes or more is 16-byte aligned, even through an `extern`, and used
  aligned SSE stores: a crash at `-O2` only. The externs are declared with alignment 1.
- **Segments and far pointers.** Far pointers are host pointers; those the game builds from
  segment numbers point into the arena, where segment arithmetic works as on the 8086. One
  quirk is kept on purpose: the moongate's stars build a far pointer from a 16-bit `y * 320`
  that wraps from line 103, so those stars land 16 pixels to the left, as in DOS.
- **Stack garbage.** The game serialised a local whose third byte it never set, so object
  files carried whatever was on the stack: replays differed between `-O0` and `-O3`. The
  port zeroes it (`PORT_ZERO`, nothing for Turbo C).
- **String literals are read-only on the host.** The conversation driver writes an NPC's
  name into a literal; the host gets a writable copy (`WRITABLE_STR`).
- **Reading its own code.** `U.EXE`'s monitor noise and the gypsy's pattern are random bytes
  of the program's own code (`MK_FP(_CS, rand())`). The port loads `U.EXE`'s image and
  points `_CS` into it, so the noise is the same.

## 3. Rewrite the assembly and drivers in C, and test them against the originals

The game links an assembly library (OSI: files, keyboard, LZW, mouse, timer, random numbers,
PC speaker) and loads binary drivers (graphics, music). All of them were rewritten in C,
and each was **tested against the original machine code run in an emulator** (unicorn):
same inputs, compare outputs, memory and, for the music driver, every OPL register write of
every song (`port/tests/*_diff.py`). This found many off-by-one and sign errors before they
could show up as a subtle visual or audio difference, and made later changes safe.

The drivers became plain C with their entry offsets kept in the names: `DRV(o)(args)` in the
game's C becomes `UDRV_o(args)` on the host by token pasting, so the call sites did not
change.

## 4. Several DOS programs in one executable

In DOS, `ULTIMA6.EXE` runs `U.EXE`, which runs `GAME.EXE`, which runs `END.EXE` at the end
(or `ULTIMA6.EXE` again when there is no character). The port links all three:

- **Name clashes**: each program defines its own `main`, `ParseConfig`, `PlayMusic`, ... A
  generated header renames every public name of `U.EXE` to `U_<name>` and of `END.EXE` to
  `E_<name>` (`urename.h`, `erename.h`). `END.EXE` links the same OSI library as `U.EXE`, so
  its library names map to the `U_` ones and it shares that port.
- **`execl`** becomes a handler: `U.EXE` running `GAME.EXE` jumps back (`longjmp`) to the
  port's `main`, which goes on with the game; `GAME.EXE` running `END.EXE` calls `END`'s
  `main` in place and exits; `GAME.EXE` running `ULTIMA6.EXE` restarts the port with the
  menu. Each program gets a fresh DOS arena.
- **Generated, not hand-written.** Renames, prototypes and BSS layouts all come from
  scripts that read the sources, so adding `END.EXE` was a `--prog end` option, not a new
  set of hand-kept lists.

## 5. Time and input

- **Timer ticks one at a time.** `TIM_calibrate` waits for the tick counter to move by
  exactly one and then counts loops until the next one. When the host's poll advanced the
  counter by two at once, it waited forever. The port's poll now delivers at most one tick
  per call.
- **Short clicks get lost.** The game looks at the mouse at intervals; a touchpad tap whose
  press and release arrive together came and went between two looks. A press now shows for
  at least 80 ms.
- **Busy waits.** Delay loops calibrated in loops per tick (`loopsPerTick`) become real-time
  waits in the port. Waiting for the vertical retrace waits for the next frame on a fixed
  70.086 Hz grid, so loops paced by it keep the original's speed.

## 6. Recording and replaying exactly

Every value the game reads from the outside world (timer ticks, keys, shift state, mouse,
random seed, date and time) goes through one function that either records it or returns the
recorded value. Every 4096 reads, a digest of the game state is stored, and a replay checks
it. What it took to make a recording made on Windows replay identically on Linux, across
gcc and clang, `-O0` and `-O3`:

- no host-dependent value may reach the game: Borland's own `rand`, packed structs, explicit
  integer widths, the DOS memory layout;
- no uninitialised memory may reach a file (the stack garbage above);
- the digests make divergence visible at the first wrong step instead of hours later;
- sessions with random input, replayed across builds (and under AddressSanitizer), are the
  test.

## 7. Sound

The AdLib driver (U6ADLIB.DRV) is C, ticked at 60 Hz inside the SDL audio callback, on an
emulated OPL chip (Nuked OPL3). Two problems, both invisible in the driver's logic:

- **Writes on the same sample.** The driver's note-on writes register B0h twice, key off and
  then key on. A real card saw them about 50 µs apart; the emulator applied both on the same
  sample, so a note never restarted and only the first notes of each song played. The
  writes now go through the emulator's write buffer, which spaces them out.
- **The audio buffer.** Missing beats and glitches remained. A DOSBox capture of the original
  (a `.dro` file) proved the port's register writes and their timing identical to DOS, so
  the problem had to be output, not music: a 512-frame (12 ms) buffer ran dry on a busy
  Windows machine. 2048 frames fixed it.

Lesson: when the output sounds wrong, capture what the original sends to the hardware and
compare that first; it separates "the logic is wrong" from "the delivery is wrong".

## 8. Building and shipping

- One CMake project builds Linux natively and Windows with mingw (cross from Linux, or
  MSYS2); the game's C is compiled as gnu89 with warnings off, the port's own code
  normally.
- A GitHub workflow builds both and publishes a release when run with a tag.
- Before a release: the DOS rebuilds (all three programs identical), the emulator diff tests,
  a run of the menu and the ending with scripted keys and screenshots on Linux and under
  Wine, and an AddressSanitizer run.
- The port reads the game's files from the user's folder and never writes there; saves,
  settings and recordings go to a home folder.

## 9. What we would do the same way again

- Never fork the game's C; make the host fit the code, and prove the DOS build unchanged.
- Rewrite the low-level pieces in C, but test each one against the original code in an
  emulator.
- Reproduce DOS's memory layout where the game depends on it, instead of fixing its
  out-of-bounds reads.
- Record every outside input from day one; deterministic replay found bugs nothing else
  would have.

## 10. What we would do differently

- Generate prototypes for every function from the start: an argument passed through an
  implicit declaration is not narrowed to 16 bits, and the resulting bugs are far from
  their cause.
- Build with `-O2`, ASan and both compilers early; optimisation-only crashes (the alignment
  case) and replay divergences show up there first.
- Test audio with a reference capture from DOSBox immediately, rather than by ear.
