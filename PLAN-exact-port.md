# Plan: an Ultima VI port proved against DOS, as Underworld Exhumed's are

Written 2026-11-15 from a read of this repository (`portable` branch), `../Exhume`
(abedegno/Exhume at `c6f7db5`) and `../underworld-exhumed` (which pins the same commit).
Nothing here has been built or run yet. Items marked **(unverified)** are from the docs,
not from reading the tool's code, and are the first things Phase 0 has to settle.

## 1. What "exact" means there, and what we have

Underworld Exhumed's claim is not "the port replays its own recordings". It is: a session
**recorded in DOS**, from the original program's code, replays in the port with the same
game state, the same video memory and the same saved-game bytes at every checkpoint, on
every supported host, on every push. The DOS run is the ground truth; the port is graded
against it.

| | Underworld Exhumed | this port today |
|---|---|---|
| DOS build byte-identical | yes, per file and whole EXE, one command (`make check`) | yes, whole EXE, three programs, three separate scripts |
| One tree, two builds | yes | yes (`portable.h`, `compat.h`) |
| Where input is hooked | in the shared C, at the program's boundary, so DOS can record too | in port-only C (`port/sys/kbd.c`, `timer.c`, `mouse.c`, `rand.c`, `dosint.c`), under the OSI library: DOS cannot record |
| Ground truth for a session | DOS, replayed twice and required identical | none: the port is compared with itself |
| What is compared | every state section, DS:0 and the vector table, the rand seed, all of video memory and the DAC, saved games byte for byte | one FNV digest of the object, NPC and party arrays every 4096 reads |
| Goldens and a test gate | committed digests, `make test` before every push, CI on three hosts | none; CI only builds releases |
| Single routines vs the original's bytes | game routines fuzzed in Unicorn, test vectors kept | the OSI library and the drivers only (good, keep) |
| Host hazard audits | null reads, written literals, reads past tables, stack junk, promotions, struct layout vs Turbo C | promotion audit exists; the rest met as crashes |
| Stack junk | `STACK_JUNK(v)`, v read from the disassembly | `PORT_ZERO`: the port writes zeros where DOS wrote stack contents (`SRC/object.c:1344`), so that file byte is knowingly not DOS's |
| `U.EXE`, `END.EXE` | n/a | not recorded at all |

So the decompilation is already at Underworld Exhumed's standard, and the port's
*construction* is the same method. What is missing is the proof: the DOS side of the
replay, full-state comparison, goldens, and the gate that runs them.

## 2. What Exhume is

Exhume (abedegno, MIT) is three things in one repository, none of them a game:

1. **Tools** (`tools/`, Python and Node): run the original DOS toolchain headless (emu2,
   DOSBox-X, js-dos), match and verify sources against an EXE, the one-command gate, and
   for ports: compile measurement and link stubs, explicit widths, promotion audit, struct
   layout check against Turbo C, record/replay/goldens, routine fuzzing and vectors in
   Unicorn, coverage, packaging, CI templates.
2. **A runtime** (`runtime/`): the half of a port that is not the game's. `portable.h`
   (shared with the DOS build), `replay.c` (compiled by the replay DOS build *and* the
   port), the portability layer, an SDL3 platform layer, emulated VGA/EMS/PIT, Borland's
   C library, the paragraph map and far heap, a Miles AIL 2 sound library, the x86 machine
   for statically recompiled assembly, the black box.
3. **Method**: per-toolchain profiles (`profiles/`), Claude Code skills per stage
   (`skills/`), and the docs (`docs/port.md` is the port method).

It was extracted from the UW2 decompilation and holds no game data or compilers.

## 3. Why Underworld Exhumed depends on it

It is a git submodule at `exhume/` (in the local checkout it is not initialised; the pin
is `c6f7db5`, the commit `../Exhume` is at). The dependency is structural, not a
convenience:

- **The DOS build includes it.** Every shared source includes Exhume's `portable.h`,
  staged into each Turbo C compile. Without the submodule the byte-identical build does
  not compile.
- **The ports are compiled from it.** `runtime/` is built in place, never copied, and is
  in the release packages (the NOTICE says so).
- **The proof is run by it.** `make check`, `make test`, the goldens, fuzzing and CI are
  Exhume's tools reading each game's `exhume.toml`.
- **The pin is part of the claim.** "Identical to DOS" was proved with one Exhume commit;
  the submodule records which.
- **Two games share one runtime**, so a fix reaches both and neither drifts.

## 4. Can Exhume power this port?

Yes for the proof, mostly yes for the platform, no for the sound, and it needs additions
upstream. By part:

| Exhume part | Fit for Ultima VI |
|---|---|
| `replay.c`, `replay.py`, `golden.py`, `replaydos.mjs` | **The reason to adopt it.** Generic by design (bindings in `rpgame.h`). **(unverified)** how much of the runtime `replay.c` needs in the port. |
| `fuzzasm.py`, `vectors.py`, `coverage.py` | Fits: runs EXE bytes in Unicorn, as our driver tests do. `GAME.EXE` is EXEPACKed; feed it our unpacked image (`tools/exepack.py`). |
| `layoutcheck.py`, `widths.py`, `intaudit.py`, `portcheck.py` | Fit once Turbo C 2.0 runs under Exhume's DOS runner. Our `port/tools/intaudit.py` already covers one. |
| Toolchain profile | **Missing.** Profiles exist for Turbo C++ 1.01 and Borland C++ 2.0; we need Turbo C 2.0 + MASM 3.0 + TLINK. Exhume's README lists it as "would come next". |
| Per-file match/verify gate | Optional. Its rules assume VROOMM overlays; ours has none and already proves whole EXEs. Wrap our checks as the gate first. |
| Platform layer, `vga.c`, `borland.c`, game finder, black box, crash, input scripts, cheats | Fit. `vga.c` handles mode 13h. Replaces `plat_sdl2.c`, `vgadac.c`, `hostfs.c`, parts of `session.c`. |
| Paragraph map and far heap | Fits in principle; we use a flat 1 MB arena. A swap, not a need. |
| Sound library | **No.** It is Miles AIL 2; Ultima VI has Origin's own `U6*.DRV`. Exclude it, keep `adlib.c`. |
| `asm2c.py` | Not needed: our assembly is already hand-written C, tested against the original. |
| Three programs in one executable | **(unverified)** the runtime assumes one `PORT_GAME_MAIN`; our rename scheme (`U_*`, `E_*`) has to sit on top. |
| Web build, packaging, signed macOS app | Fit, late. |

## 5. The plan

Order differs from Exhume's own (platform, then replay) on purpose: we already have a
working platform, so the proof comes first and then guards the platform swap.

### Phase 0: one gate, Exhume pinned (small)
- Add Exhume as a submodule at `exhume/`, pinned. Add `exhume.toml` and a `Makefile`.
- Write the `borland-tc20` profile (command lines, name rules) so Exhume's DOS runner
  compiles with our Turbo C 2.0 and MASM 3.0.
- `make check` = `GAME.EXE`, `U.EXE`, `END.EXE` identical, by wrapping `dosbuild.py` and
  `ubuild.py`.
- Spike the two unverified items: `replay.c` built for the port without the rest of the
  runtime, and for DOS under Turbo C 2.0.
- **Exit:** `make check` green from a clean clone; both spikes answered.

### Phase 1: DOS becomes the ground truth (the crux)
- Move every outside read up into the shared C as a `portable.h` hook that is the
  original tokens under Turbo C: keys (`KBD_GetKey`), the tick counter, mouse position
  and buttons, shift state, the time seed (`OSI_timeSeed`), DOS date and time. The gate
  proves each costs no byte.
- Find what interrupts change behind the game's back (timer tick, mouse handler, the
  music driver's 60 Hz service) and route it through hooks. This is the hardest part.
- Write `rpgame.h`: the dump's sections (saved-state block, object and NPC arrays, the
  whole uninitialised data block, rand state, DS:0, the 320x200 frame and the DAC).
- Replay DOS build: the normal link with `-DREPLAY` and `replay.obj` (no overlays, so no
  modding link is needed).
- Replace `PORT_ZERO` with `STACK_JUNK(v)`, v read from our own build's disassembly.
- **Exit:** one session recorded in DOS replays in DOS twice with identical dumps.

### Phase 2: the port against DOS
- The port compiles the same `replay.c`; our `port/replay/` and `digest.c` go.
- Compare checkpoint by checkpoint; fix the port, mask only machinery, each mask with its
  reason. Expect divergences where the port invented timing: the one-tick-per-poll
  timer, the 80 ms mouse press, retrace waits, calibrated delay loops.
- **Exit:** that session identical at every checkpoint, the frame and palette included,
  and the saved game byte for byte, on Linux and Windows, gcc and clang.

### Phase 3: sessions, goldens, the test gate
- About ten sessions of ordinary play: walking and day/night, a long conversation,
  melee and ranged combat, spells, camping, a dungeon, a boat and a moongate, shops,
  save and reload.
- Goldens from DOS (twice, identical) committed with their recordings.
- `make test` = gate + port build + every session against its golden; pre-push hook; CI
  on Linux and Windows from an encrypted bundle (Exhume's ci-bundle).
- **Exit:** CI red on any checkpoint difference.

### Phase 4: `U.EXE` and `END.EXE` under the same proof
- Each is its own DOS program: its own hooks, replay build and sessions (the gypsy, the
  introduction, a transfer from Ultima IV and V, the ending).
- **Exit:** no part of the port is unrecorded.

### Phase 5: what the sessions do not reach
- Audits over all three programs: struct layout against Turbo C 2.0, null reads (what
  DS:0 holds under Turbo C 2.0's start-up), literals written through, ASan and
  array-bounds builds over every session, `-ftrivial-auto-var-init=pattern` for more
  stack junk.
- Fuzz targets and vectors for game routines no session pins down: combat rolls, line of
  sight, path finding, the conversation interpreter's operators.
- Fold the existing Unicorn driver tests into `make test`; publish coverage.
- **Exit:** every routine is either reached by a session, fuzzed, or listed as unverified.

### Phase 6: Exhume's runtime under the port
- SDL3 platform layer, emulated VGA with scan-out, Borland library, game finder, black
  box, crash handler; `mcga.c` writes into the emulated VGA. Sound library excluded.
- Then, if wanted: the Roland driver through libmt32emu, checked byte for byte as
  `adlib_diff.py` checks AdLib.
- **Exit:** every golden unchanged across the swap.

### Phase 7: release
- Packages for Windows, Linux and macOS, the web build, fixes from `FINDINGS.md` as
  options that are off by default (replays run with all off).

## 6. Decisions that are yours

1. **Exhume changes** (Turbo C 2.0 profile, perhaps several programs per port):
   **decided 2026-11-15: Karl's fork of Exhume.** The submodule at `exhume/` points at the
   fork; additions go on a branch there, kept rebased on abedegno's `master` so they can
   be offered upstream later. Generic runtime fixes are still worth proving on UW2
   (`examples/uw2/prove-port.sh`) before the pin moves.
2. **Phase 6 at all?** Phases 0 to 5 give the Underworld Exhumed standard of proof on the
   current SDL2 platform. Phase 6 buys shared maintenance, macOS and the web build.
3. **Where the tools run.** Exhume's tools are shell, Python and Node, used on macOS and
   Linux; here that means WSL, as the Underworld work already does.

## 7. Phase 0 results (2026-11-15)

Done by three workers, one at a time, each reviewed and its proof rerun. Nothing committed.

- **Profile.** `../Exhume`, local branch `u6-tc20`, uncommitted: `profiles/borland-tc20`
  and about 40 lines in `tools/build.py`, `config.py`, `dosrun.mjs`. Exhume's build path
  compiles and assembles our sources to objects byte-identical to `dosbuild.py`'s (3 C
  files and 4 MASM modules of GAME; 2 C files of U once the recorded source times agree).
  Not tried through Exhume: the link, huge-model files, inline-assembly C.
- **Gate.** `python tools/gate.py check` (Git Bash, 22 s) or `make check` (WSL, 90 s):
  three programs, sha1s pinned. `make test` adds the port and the Unicorn driver tests.
- **Spikes.** Experiments in the session scratchpad (`spike/RERUN.txt`).
  - `replay.c` compiles unmodified under Turbo C 2.0, with GAME's switches and U's.
  - **It does not fit as it is.** GAME's DGROUP ends at EDFA: 4,614 bytes are left for
    stack and near heap, and stock `replay.c` has 6.3 KB of `_BSS`. The link fails.
  - **Adding it moves the game's data.** Any initialised data in `replay.c`, or in a
    library module it pulls in, shifts the whole `BSS.ASM` block (by 400 bytes in the
    slimmed trial). Near pointers stored in game state then differ from the real game's.
  - `replay.c` can go into the current SDL2 port ahead of the rest of the runtime, with a
    shim of about 50 lines. A stand-in game recorded and replayed to an identical dump.
  - The stack-junk byte (`object.c:1344`) cannot be determined: interrupts write it. It
    reaches saved games. `STACK_JUNK_SET(containerPos.raw[2], 0)` compiles to the original
    bytes, so the replay build and the port both write 0.
  - **Outside reads the port does not record** (it fakes them as constants):
    `farcoreleft()` (`main.c:522`), which sets how many tile-cache pages the graphics
    driver gets and so when `CurrentDisk` changes; DOS version, drive types and current
    drive (`disk.c:181-231`); `CpuSpeed`; int 12h. `RP_SHIFT` and `RP_DATE` in the port
    are never reached by GAME.
  - The mouse is polled inside assembly that also calls back into C and redraws, so it
    needs a replay-build-only `MOUSE` module, not a C macro.
  - U.EXE reads its own code bytes as noise, so its replay build's noise differs from
    U.EXE's; it also runs game code from timer callbacks.

## 8. Phase 1, revised from the spikes

Design decision (coordinator): the replay build must not move the game's data, so the
harness keeps **nothing in DGROUP**. Its buffers and strings go in a far segment defined
by a small MASM module and reached through far pointers (Turbo C 2.0 refuses `far`
globals in the medium model, but accepts far pointers), and its file I/O goes through the
already-linked `intdos`, so no library data is pulled in. Exit test for that: the map's
`_DATA` and `BSS.ASM` offsets are the same with and without the harness.

1. Exhume's `portable.h` under the project's own; `STACK_JUNK_SET` replaces `PORT_ZERO`.
   Exit: gate. *Easy (Sonnet).*
2. Measure GAME's real stack depth in DOSBox-X with a pattern-fill probe. Exit: a number
   with margin. *Medium (Sonnet).*
3. `replay.c` for Ultima VI in the fork: far-segment storage, no DGROUP data, the seven
   binding changes the spike lists (seed location, null checksum 0DFAh, stop key,
   `KEY(arg)`, a generic recorded value, I/O macros). Exit: GAME links with data unmoved;
   UW1 and UW2 replays unchanged. *Hard (Opus).*
4. Hooks in GAME's C: keys (`console.c:564`, `main.c:235`), seed (`main.c:510`),
   `farcoreleft` (`main.c:522`), DOS and drive queries (`disk.c`). Exit: gate, no byte
   changed. *Medium (Opus).*
5. Replay-build-only `MOUSE` and `OSI_delay` modules; `CpuSpeed` recorded. Exit: the
   replay build runs in DOSBox-X. *Medium (Opus).*
6. `rpgame.h`: dump sections, masks, far-block segments. Exit: two DOS replays of a
   trivial session identical. *Hard (Opus).*
7. Replay link script, `[replay]` in `exhume.toml`, a stop key the game can see, a staged
   saved game with a character. Exit: `replay.py build` and `dos` run. *Medium (Sonnet).*
8. First real session: record in js-dos, replay twice in DOSBox-X, identical. *Hard
   (Opus).*

## 9. Phase 1 task 2: GAME's stack depth (2026-11-16)

Measured with a probe build (pattern A5 filled below the frame at the start of `main` and
again before `GameLoop`; the lowest overwritten byte read at every `CON_getch` and every
turn), headless DOSBox-X, AdLib music on, a saved game with a party of four in a fight.
The probe adds no data: `_BSSEND` is at DGROUP:EDFAh in both maps (checked in review).

- **Room.** The stack runs from 10000h down; the near heap above EDFAh holds only C0's
  copies of the arguments and environment (160 bytes under DOSBox-X, more with a larger
  environment), and the game never calls `malloc`. Usable: 4,454 bytes here.
- **Measured.** Deepest seen 1,004 bytes; every run that drew a missile 956 to 990;
  walking, time passing and conversation without one 255 to 409. The peak is `Missiles()`
  (`gfx.c`), a 616-byte frame.
- **Not reached by any run:** spell casting, rest, a town, save and load, explosion chains.
  The measured figure is therefore a floor, not a ceiling.
- **Static.** Longest simple call path 2,134 bytes (not feasible as a whole; plausible
  chains about 1,600 to 1,900). Recursion exists (a 69-function component; the cheapest
  cycle is 44 bytes a level, the one through `Missiles` 878) and its depth is not bounded.
  203 indirect call sites and the drivers' interrupt handlers are not followed.

**Budget for the harness (coordinator's reading, stricter than the worker's 1 KB):** a
hook may use at most **256 bytes** of stack including its callees and the DOS call, never
recurse, never call `printf`-class library routines. With far buffers nothing more is
needed, and 2,134 + 256 + interrupts stays under 2.6 KB of 4,454. Task 6 adds a stack
check to the replay build (the same pattern fill, lowest address written to the dump), so
every recorded session measures its own depth and the unreached paths get covered as the
sessions grow.

Rerun: scratch only (`probe_patch.py`, `sessions.py`, `static.py`); the method moves into
the replay build in task 6, not into the tree as is.

## 10. Phase 1 task 3: `replay.c` for Ultima VI (2026-11-16)

In the fork, commit `e0039a6` (local; the submodule is pinned to it). With
`-DRP_FAR_STORAGE` the harness keeps every variable and string in a far segment `RP_FAR`
(`runtime/replay/rpfar.asm`, class `'CODE'` so that TLINK places it before DGROUP), does
its file I/O through int 21h and has no near data: linked into GAME.EXE with a stub
caller, `_DATA` (3228h), `_BSS` (BBCCh) and `_BSSEND` (EDFAh) are unchanged and all 340
DGROUP publics keep their offsets. Stack per entry point: at most 236 bytes (static, every
path), 154 measured in emu2.

Bindings for the later tasks: `RP_RAND_SEED()`, `RP_NULL_SUM` 0DFAh with
`RP_NULL_SUM_TO`, `RP_STOP_KEY(r)`, `RP_SRAND`, `RP_WALL_READ`, the I/O and buffer-size
macros, `RP_MAGIC_TAG`; in `hookgame.h`, `HOOK_KEY_ARG` (the hook is `KEY(a)`) and
`HOOK_RECORDED` (`RECORDED(id, x)`, `rp_value`, MISC tag 10).

Underworld: its replay objects are identical but for COMENT records (review made
`rp_value` conditional on `HOOK_RECORDED` to get that); the UW1 and UW2 ports replay all
17 and 13 sessions identical to DOS. Not run: Underworld's own DOS-side golden check.

Open for tasks 4 to 7: `RP_KEYSTATE_LEN` must be at least 1 and Ultima VI keeps no key
state; `RP_FAR_SIZE` lives in both `replay.c` and `rpfar.asm` (MASM 3.00 has no `/D`);
in far mode the harness's variable names are macros and can capture a game variable of the
same name inside a binding's `RP_DUMP`; `rpfar.asm` is not yet in `tools/replay.py build`.

## 11. Phase 1 task 4: the hooks in GAME's C (2026-11-16, done by the coordinator)

`SRC/hookgame.h` now defines `HOOK_KEY_ARG`, `ORIG_KEY(a)`, `HOOK_RECORDED`, the ids of
the recorded values and `RECORDED16` (a recorded value cast back to int: `rp_value` gives
a long, which Turbo C would push whole to a function without a prototype).

| Where | What | Hook |
|---|---|---|
| `console.c` `CON_getch`, `main.c` the disk error prompt | `KBD_GetKey(a)` | `KEY(a)` |
| `main.c` `main` | `OSI_timeSeed()` | `RECORDED16(RV_SEED, ...)` |
| `main.c` `main` | `farcoreleft()` | `RECORDED(RV_FARCORE, ...)` |
| `disk.c` `DISK_init` | DOS version (AL, AH); per drive IOCTL 4408h carry and AX, 440Eh AL; int 11h AX; `OSI_getDrive()` twice | `RECORDED16(RV_..., ...)` at each use |

Nothing else in GAME's C reads from outside the program: the two int 10h calls in `main.c`
set the palette and read nothing back; every other read is in the assembly library
(task 5: the clock, the mouse, the delay calibration, the file reads' errors).

Exit tests: the gate passes (the hooks are the original tokens in the DOS build). The three
files and `replay.c` compile with `-DREPLAY` under Turbo C 2.0 and link into GAME.EXE with
`rpfar.asm`: `_DATA`, `_BSS`, `_BSSEND` and all 340 DGROUP publics unmoved. The port builds
with pass-through `rp_key` and `rp_value` (`port/sys/rphooks.c`, until Phase 2 links
`replay.c`) and replays a scripted session with all 225 checkpoints identical.

Not proven here: that the hooks record and replay correctly in DOS. No replay build runs
yet (it needs task 5's modules and task 6's `rpgame.h`; the link above used task 3's stub
bindings).

`tools/dosbuild.py` now stages `hookgame.h` with CRLF line ends: git stores this one file
with LF, and Turbo C 2.0 reads an LF-only header as one line.

## 12. Phase 1 task 5: the assembly library's outside reads (2026-11-25)

Done by one worker, reviewed; the gate and the map check rerun by the coordinator.

- `SRC/OSILIB/MOUSERP.ASM`, the replay build's MOUSE.ASM (its data segment the same byte for
  byte): the reset's AX and BX through `rp_value` (`RV_MOUSE_RESET`, `RV_MOUSE_NBTN`); each
  poll through `rp_mouse` then `rp_mbuttons`, the one int 33h made by `u6_mouse_poll`
  (`RP_MOUSE_READ`) into three words in the code segment. x is stored halved, as the game uses
  it, so the stream is in 320-pixel coordinates the port can give.
- `SRC/OSILIB/DELAYRP.ASM`: the int 1Ch handler counts in a code-segment word, so `D_305A`
  stays 0 in every dump. `OSI_delay` still waits in real time while replaying (follow-up: an
  `rp_mode` accessor in the fork would let it return at once).
- `RECORD_VAR(id, lv)` (hookgame.h), empty in the DOS build: `CpuSpeed` (`RV_CPUSPEED`) and
  int 12h's `D_2FC8` (`RV_MEMSIZE`, read nowhere but in the dumps) after `OSI_lowInit`.
  `D_2FC8` and `D_3088` (the rand state, for `RP_RAND_SEED`) made PUBLIC; the EXE is unchanged.
- `SRC/rpgame.h`, minimal: stop key F9 (143h; int 16h AH=0 never gives F11/F12), the mouse,
  key state and joystick lvalues outside DGROUP, `RP_SHUTDOWN` = `rp_u6_shutdown` (main.c,
  REPLAY only). `RP_CLOCK`, `RP_TICK_US`, `RP_PIT_DIVISOR` and the dump (DS:0..EDFAh plus the
  common sections) are placeholders marked TASK 6.

Exit tests: gate PASS (3 of 3). Replay link: `_DATA`, `_BSS`, `_BSSEND` and all 342 DGROUP
publics unmoved. Headless DOSBox-X, a staged character (an OBJLIST made by the port): recorded
`space up up left f9`, stopped at hook call 153198, RECORD.OUT 338 bytes with every VALUE
(CpuSpeed 672, memory 640, mouse FFFFh/3 buttons) and the MOUSE and BUTTONS streams. Replayed
from it: ran to the end, no desync; all 6 dumps equal to the recording's, VGA, PAL and RAND
included, except DS:0098, C0's `__STARTTIME` (the BIOS tick at start-up): task 6 masks it.
Port: builds, a scripted session 707 of 707 checkpoints identical. Stack (static, every path):
rp_mouse 212, rp_mbuttons 208, rp_value 168 bytes, within 256.

Other outside reads found, left for later: the int 24h handler's `D_2FCC`, write errors via
the OSI_FILE callbacks, LOW's calibration counter `D_2FCA` (ends at 1), the music driver's
timer service and the graphics driver (task 6).

Rerun: scratch only (`rpbuild.py`, `rprun.py`, `rpdecode.py`, `rpcmp.py`, `rpstack.py`); the
link becomes task 7's.

## 13. Phase 1 task 6: the dump (2026-11-26)

Done by one worker, reviewed; the gate, the map check and the replays rerun by the coordinator,
who changed the stack fill (below).

`SRC/rpgame.h` dumps, in order (bytes with VGA, mouse, AdLib under DOSBox-X):
- **Once the graphics driver has its memory (`RP_LEVEL`):** GSTA (DS:2C4A..2CCB, 130), OBJS (the
  five object arrays, 30720) and NPCS (the rest of OBJLIST's NPC tables and the party, 5105).
  Together they are a saved game's OBJLIST byte for byte.
- **Always:** DATA (DS:0..322D, 12846) and BSS (DS:322E..EDF9, 48076).
- **The far heap, once the graphics driver has its memory:**
  - SEGS: 30 segment words, for `[replay.segments]`.
  - GDRV (the mouse buffers and the graphics driver, 16888).
  - MDRV (the music driver, 1662).
  - FARB (the game's far blocks, 60098).
  - GRMM (the graphics driver's tile cache, 270544).
- **Always:** STAK (the stack check, 6), RAND, NULL, CNTS.
- **In every dump, since every Ultima VI dump is full:** PAL and SCRN, a linear
  A000:0000 frame of 64000 bytes. Chain-4 makes `dump_screen`'s four planes the same 64 KB four
  times, and `replay.py`'s scanout cannot draw them; `replay.py` needs a SCRN reader (task 7
  or Phase 2).

Masks, listed for `[[replay.mask]]` (task 7 writes them into `exhume.toml`), none zeroed:
- DATA 98h..9Bh, C0's `__STARTTIME`, which nothing reads.
- MDRV 0Ch..161h, U6ADLIB.DRV's variables, stepped by its 60 Hz int 8 handler. The offsets are
  AdLib's; another music driver needs its own.
- STAK 4..5, the lowest address written, which is a measurement.

`RP_CLOCK` is the BIOS tick, but no hook reads it: Ultima VI never calls GAME_TIME, so there
are no periodic dumps. Dumps come at each key, each button change and the end; `UWRPHOOK`
bisects. Possible later: a CHECKPOINT at each turn, for long AI-only stretches.

Stack check: `SRC/OSILIB/STACKRP.ASM` (replay build only, code-segment storage) fills from
`__brklvl` (EE9Ah) to 40 bytes below the first hook's SP (FFB0h), through a new optional binding
`RP_START()` in the fork (`46c7857` on `u6-tc20`, +5 lines, Underworld unaffected). The worker
filled with A5h. The coordinator changed it to **zeros**: any pattern changes what the game's
uninitialised locals read, so the replay build would no longer behave as GAME.EXE (DOSBox-X's
memory is zero there, and the port's stack junk is 0). The lowest non-zero byte is a floor, and
it reads the same here: lowest FC22h, 990 bytes below 10000h, matching section 9.

Exit tests:
- Gate PASS.
- `_DATA`, `_BSS`, `_BSSEND` and all 342 DGROUP publics unmoved.
- Worker's session `rec7` (moves, a look, a failed talk, turns passing; 27 dumps of 512 KB):
  two replays identical apart from the masks, and so are the recording and its replay
  (rerun by the coordinator).
- After the zero fill, the coordinator's session `z1` (moves, a look, a conversation with
  name, job and bye, turns passing; 30 dumps): the same result.
- The port builds; a 60 s scripted session matched at 927 of 927 checkpoints (worker).

Open:
- One live recording by the worker hung after "bye" (A5h fill, killed at 400 s, no
  recording). The same keys completed on both fills afterwards; the cause is unknown.
- Only AdLib and VGA were tried.

Rerun: scratch (`task6/RERUN.txt`: rpbuild, rprun, rpcmp, rpdecode, rpstack, shots); the build
becomes task 7's.

## 14. Phase 1 task 7: the replay build and its runs as tools (2026-11-26)

Done by one worker, reviewed; the gate, a clean build and `rp.py check` rerun by the coordinator.

- `exhume.toml` (new): `[replay]` with the three masks and their reasons, `[replay.segments]`
  (DGROUP paragraph 329Bh; untested until Phase 2), stop key F9, the DOSBox-X conf.
- `tools/rpbuild.py`: GAME.EXE checked against the gate's sha1, then RGAME.EXE. It fails if any
  DGROUP public moved, and is cached by a sha1 over its inputs. Exhume's `replay.py build` runs it
  through a new `[replay] build` hook.
- `tools/rp.py`: `build`, `record` (ours: DOSBox-X AUTOTYPE, F9 appended; `replay.py record`
  needs js-dos), `replay`, `compare`, `show`, `log` (Exhume's `replay.py`), and `check` (record,
  replay twice, compare both pairs, PNGs). `make replay`, `make replay-check`. `docs/replay.md`.
- `tests/replay/`:
  - `stage/SAVEGAME/OBJLIST`: the character, made by the port's `--new-avatar`, reproducible
    byte for byte. Mostly the installed game's own starting OBJLIST.
  - `sessions.toml`: `smoke`, a minute of the opening fight.
  - `dosbox-x.conf`: replaydos's default. Recording and replay use the same conf, switches and
    batch, since a different start-up moved the load segment by a paragraph.
- Fork `dd92217` on `u6-tc20`: `[replay] build`, `stop_key`, and the SCRN frame in
  `scanout`/`compare`/`show`. All of it is inactive unless configured. On synthetic Underworld
  dumps the output is unchanged (Exhume has no tests of `replay.py`).

Exit tests:
- Gate PASS.
- A clean `rp.py build`: DGROUP unmoved, RGAME.EXE sha1 `def703cb...`, the same as task 6's
  scratch build.
- `rp.py check`: 27 dumps; recording = replay 1 = replay 2 apart from the masks, read from
  `exhume.toml`. With the masks removed, DATA 98..99 differs.
- The last PNG is the throne-room fight with the party panel.
- Port: 1108 of 1108 checkpoints identical (worker).

Open, for task 8:
- **Recordings that hang at a conversation:**
  - once in task 6 (after "bye");
  - twice here, both at the same byte of STATE.OUT, while the conf began with a comment block,
    and never without it.

  The cause is unknown, and it could be the game, the harness (a dump of half a megabyte at each
  key, the stack fill) or DOSBox-X. Task 8 should find it before relying on long sessions.
- A WSL (snap DOSBox-X) replay of a Windows recording took the same path but differed in a few
  DATA/NULL/STAK/BSS bytes, so compare only within one DOSBox-X install.
- Golden PNGs, checking the saved games, js-dos recording (`stop_key` is passed, untested).

## 15. Phase 1 task 8: scripted character creation and the first real session (2026-11-26)

Done by one worker and reviewed. The coordinator reran the gate, a fresh `rp.py chargen --force`, `rp.py check --session first` and `--rec tests/replay/first.rec`.

- **The character comes from a script, as Underworld Exhumed's `_chargen` does.**
  - `[_chargen]` in `tests/replay/sessions.toml` drives the real U.EXE in DOSBox-X: Esc, C, "Avatar", male, the first portrait, then A to all seven questions.
  - The result is Avatar, strength 17, dexterity 20, intelligence 22. OBJLIST is 7539 bytes, sha1 `01572292...`. `rp.py` checks it against `objlist_sha1` and fails loudly on a mismatch.
  - It is cached in `build/dos/replay/stage` by a sha1 of its inputs.
  - While U.EXE runs, GAME.EXE is a 63-byte stub that exits.
  - The committed `tests/replay/stage/OBJLIST` is gone.
- **Why U.EXE needs help:** `srand(time(0))` shuffles the gypsy's questions, and `DrawPattern` calls rand() every frame after the first answer. No fixed answers give the same character under every order.
- **`tools/keyfeed.py` builds KEYFEED.COM**, a small TSR that the Python assembles itself; `--list` prints the listing. It types a key script counted in the program's own keyboard polls, not in time, and it gives the program a fixed date and time.
  - Mode `dos`: int 21h 0Bh, 07h and 08h, for U.EXE.
  - Mode `bios`: int 16h 01h/11h and 00h/10h, for GAME. This mode also fixes OSI_timeSeed's seed and can type Ctrl+S.
  - It is loaded with `LH`, so conventional memory matches a replay. A replay doesn't load it: the keys come from RECORD.OUT.
  - Two new masks, NULL 8B..8E and B7..BA, cover the int 16h and int 21h vectors, which point into KEYFEED only while recording. The game neither sets nor reads them.
- **The conversation hang was DOSBox-X's AUTOTYPE**, not the game or the harness:
  - 3 of 37 AUTOTYPE recordings hung.
  - In one, the game kept polling (2,484 empty KEY polls) and the last keys never reached the BIOS buffer.
  - In another, DOSBox-X itself froze.
  - The comment-block link was coincidence. It did expose a bug: `str.replace('[cpu]')` wrote cycles into a comment. This is now a section-aware `conf_set`.
  - With KEYFEED, the formerly hanging setup recorded 5 of 5 runs, all with the same RECORD.OUT.
- **The first real session is `[first]`, recorded as `tests/replay/first.rec`.** It covers:
  - a talk with Dupre (name, job, join, bye);
  - the inventory (F1, F10);
  - a look at Lord British;
  - a fight with six attacks north;
  - six steps out of the throne room and ten passes;
  - a save with Ctrl+S, Y.

  It has 70 dumps. A fresh recording is byte-identical to `first.rec` (`679203c9...`).
- **`rp.py check` now compares the saved games** too (`[replay] saves = ["SAVEGAME"]`).
- **Recorded in DOSBox-X, not js-dos:** js-dos loads GAME 680h paragraphs lower, so the recorded `farcoreleft` differs and a DOSBox-X replay stalls.

Exit tests:
- Gate passes, 3 of 3.
- chargen: the same OBJLIST sha1 on every run, including runs at fixed cycles, at max cycles and under WSL's DOSBox-X. A different clock gives a different character, which shows the clock really is the seed.
- `rp.py check --session first`: recording = replay 1 = replay 2 apart from the masks, and the saved games are identical. `--rec first.rec` passes. `make replay-check` passes in WSL.
- Port: builds; a 60 s scripted session replays 1510 of 1510 checkpoints identical (worker).

This closes Phase 1. Open:
- Why AUTOTYPE fails inside DOSBox-X: untraced, and avoided by using KEYFEED.
- js-dos recordings replayed in DOSBox-X: the load segment differs.
- The session has no outdoor walking and no day turning to night.
- `[replay.segments]` is untested until Phase 2.
- Golden PNGs.
- Only OBJLIST is checked. Sessions still take the other SAVEGAME files (the OBJBLKs) from the user's install unchecked.
