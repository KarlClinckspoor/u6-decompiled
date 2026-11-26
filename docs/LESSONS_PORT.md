# Lessons learned: porting Ultima VI to Windows and Linux

The native port (`port/`, see `docs/port/PORT.md`) compiles the game's own decompiled C,
the same files that still build byte-identical DOS programs, for 64-bit Windows and Linux on
SDL2. It runs all three programs of the game, `U.EXE` (menu, introduction, character
creation), `GAME.EXE` and `END.EXE` (the ending), in one executable, with the AdLib music, a
settings screen (F11), and a recording of every session (Exhume's format, the replay DOS
build's) that replays exactly on any platform. This file collects what the port taught.

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
  port zeroes it (first `PORT_ZERO`, now Exhume's `STACK_JUNK_SET`: nothing for Turbo C).
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

Every value the game reads from the outside world goes through a hook that either records it
or returns the recorded value. The port first had a recorder of its own, below the game (timer
ticks, keys, shift state, mouse, seed, date, a digest of the game state every 4096 reads). It
now uses Exhume's `replay.c`, the recorder the replay DOS build runs, with its hooks in the
game's own C (the key read, the mouse, `RECORDED` values): one format for DOS and the port, and
state dumps that compare section by section instead of one digest. What it took to make a
recording made on Windows replay identically on Linux, across gcc and clang, `-O0` and `-O3`:

- no host-dependent value may reach the game: Borland's own `rand`, packed structs, explicit
  integer widths, the DOS memory layout;
- no uninitialised memory may reach a file (the stack garbage above);
- checks along the way (digests, then dumps at every input) make divergence visible at the
  first wrong step instead of hours later;
- sessions with random input, replayed across builds (and under AddressSanitizer), are the
  test.
- **Record where the original decides, not where the port reads.** The old recorder took
  the timer ticks, because the port's clock is real time. But nothing the game decides reads
  them, and DOS's recordings replay without them. Recording at the game's own hooks instead of
  the port's inputs made the port's recordings DOS's format, and dropped a stream.
- **Test scripts must not race the program.** A key script on real time lost keys whenever the
  game was busy: `KBD_GetKey(1)` keeps only the last key in the buffer, and a save typed as
  ctrl+s, Y happened in one run out of three. A key now waits until the game has taken the one
  before, as KEYFEED.COM does in DOS.

### Replaying the original DOS build (Exhume's replay build, docs/replay.md)

The same recording also has to replay in the original DOS program, built with Exhume's hooks
and run in an emulator, before it can prove the port. Emulators turned out to be part of the
program's input:

- **Type keys in the program's time, not in real time.** DOSBox-X's AUTOTYPE types from a host
  thread: keys arrive after a number of frames that varies with the host's load. They were lost
  now and then, and in 3 of 37 runs the recording hung. U.EXE seeds rand() from the clock and
  calls it every animation frame, so even the character creation came out different each time.
  A small TSR of ours, KEYFEED.COM (`tools/keyfeed.py`), offers each key after a fixed number of
  the program's own keyboard polls and reports a fixed date and time. Every recording of a
  script is now the same file, on any host, at any emulator speed. The scripted character
  creation (`[_chargen]`) is checked against a sha1, so a change in the game files or the
  emulator fails loudly.
- **Where DOS loads the program is input too.** GAME records how much far memory is free, so
  a recording only replays in a DOS that loads GAME at the same segment. Out of the box
  DOSBox-X loaded it 26 KB higher than js-dos, and a js-dos recording stalled in DOSBox-X. Two
  causes, both found in DOSBox-X's source rather than by trying settings:
  - When `minimum mcb free` is 0, DOSBox-X pads low memory with a DOS-owned block up to segment
    700h. With its kernel tables moved to upper memory and that floor set, the program's PSP
    lands where js-dos puts it.
  - The shell's environment differs (a longer PATH, a PROMPT, 1024 bytes of slack per block
    against mainline DOSBox's 83), and Turbo C's start-up sizes the near heap by the
    environment.

  Matching both (`tests/replay/dosbox-x.conf`, `[replay] dos_env`) makes the two DOSes hold
  identical game state at every checkpoint, in either direction.
- **Measure the layout with a probe, not with the game.** A 60-byte COM that writes its PSP
  segment, memory top and environment block answers in a second whether two configurations
  agree. A full replay takes minutes and fails far from the cause.
- **What can never match is the BIOS.** The C runtime saves the vectors of int 0, 4, 5 and 6
  at start-up. They and the vector table point into each emulator's own BIOS, and differ
  between js-dos, DOSBox-X 2022 and DOSBox-X 2026. Everything else matched, so these are the
  only ranges a comparison across emulators needs to mask.
- **Order matters in a DOS environment.** SET moves a variable to the end, and a short PATH set
  too early hides the emulator's own MOUNT. The same SET lines, in the same order, run in the
  batch after the mount, give every DOS the same block whatever it started with.

### The port against DOS (Phase 2)

- **Lay out the state the way DOS does, rather than masking it.** The port writes DGROUP and the
  graphics driver's segment in DOS's byte layout, every pointer in DOS's form. A generated
  table maps DOS offsets to port symbols. Then the comparison covers the program's own data,
  and what stays masked is only what the port has no counterpart for.
- **The first dump is the hardest.** It comes before the game owns the machine: the VGA is in
  the BIOS's text mode, and the far blocks don't exist yet. A small model of what DOS's machine
  holds at that point (the mode-3 DAC, an unmapped A000h) is cheaper and more honest than
  masking the whole screen.
- **A wait that decides nothing need not be waited out.** OSI_delay's tick count reaches no
  game decision, so the port's replay of a DOS recording runs on a virtual clock: 1 second
  instead of 3 minutes. That makes checking four builds after every change cheap.
- **Four compilers, one result.** gcc and clang on Linux and on Windows (where `long` is 32
  bits) give byte-identical dumps. The port's fixed-width types and its 16-bit arithmetic
  wrappers hold up.
- **Uninitialised locals show up as soon as a session reaches them.** The drake session fired a
  cannon whose ball the bars stopped before it was drawn. The missile animation still added the
  ball's never-set screen position to its dirty rectangle, and DOS and the port read different
  stack junk. The screens matched; only the dump caught it. The fix gives the local a fixed
  starting value in the replay build and the port, and nothing under Turbo C (`STACK_JUNK_FILL`,
  after Exhume's `STACK_JUNK_SET`), so the original's bytes stay the same.
- **Don't hard-code what a build can move.** The fix grew the replay build's code by 64 bytes,
  and the generator of the port's DGROUP table had DGROUP's paragraph written in as a constant:
  it silently matched a quarter of the symbols. It reads the paragraph from the map now. Two
  more copies of the same number turned up, in a static function's address and in the
  comparison's segment rules, and every recording had to be made again, since GAME records how
  much memory is left above it. Keep one copy of such a number, and check the others against it.
- **An exec frees memory.** In DOS, a program that runs another gives its memory back. The
  port runs U.EXE, GAME.EXE and END.EXE in one process, and GAME takes the whole far heap. Its
  "init" before END.EXE did nothing once the arena existed, so the ending failed with no
  memory. Nothing tested it until a session reached the end of the game.
- **Don't cache inside a folder the build empties.** The character stage lived in the replay
  build's folder. A rebuild that happened after the cache check deleted it, and the replay
  failed now and then, which looked like flakiness.

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
- Treat the emulator as input: drive it by the program's own polls (not real time), and pin
  the memory layout and the environment, so a recording is the same file everywhere and replays
  in any DOS.

## 10. What we would do differently

- Generate prototypes for every function from the start: an argument passed through an
  implicit declaration is not narrowed to 16 bits, and the resulting bugs are far from
  their cause.
- Build with `-O2`, ASan and both compilers early; optimisation-only crashes (the alignment
  case) and replay divergences show up there first.
- Test audio with a reference capture from DOSBox immediately, rather than by ear.
