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
