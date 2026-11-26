# The replay DOS build

A build of GAME.EXE that records every value the game reads from outside (keys, the mouse, the
seed, the DOS and drive queries, `farcoreleft`, the CPU speed) and can replay a recording,
writing the game's state at each key to `STATE.OUT`. Two replays of one recording must agree;
later (PLAN-exact-port.md, Phase 2) the port replays the same recordings and must agree with DOS.
The harness is Exhume's `runtime/replay/replay.c`; the game's side is `SRC/rpgame.h` (the dump's
sections and the masks, documented there) and `SRC/hookgame.h` (the hooks).

## Commands

From the repository's root, in Git Bash on Windows or in WSL/Linux:

| | |
|---|---|
| `make replay` or `python tools/rp.py build` | build it into `build/dos/replay/GAME.EXE` (skipped when nothing changed; `--force`) |
| `python tools/rp.py chargen [OUT]` | the character every run starts from: U.EXE run by script (cached in `build/dos/replay/stage/`) |
| `python tools/rp.py record OUT [--session NAME \| --keys "..." [--feed N]]` | record a session (default: `smoke`) |
| `python tools/rp.py replay OUT/RECORD.OUT OUT2` | replay it |
| `python tools/rp.py compare OUT OUT2` | compare two dumps, masks applied |
| `python tools/rp.py show OUT2 PNGDIR` | list the dumps, write each one's screen as a PNG |
| `python tools/rp.py log OUT/RECORD.OUT` | what a recording holds |
| `make replay-check` or `python tools/rp.py check OUT [--session NAME \| --rec REC]` | all of it: record (or take REC), replay twice, compare the recording with the first replay and the two replays, and their saved games, PNGs of the second |

`rp.py` is a front end. Building, replaying, comparing, showing and the log are Exhume's
`exhume/tools/replay.py` with `--config exhume.toml`; recording is `rp.py`'s own.

Needs: Python 3.11+, Node.js (Exhume's `replaydos.mjs` runs the replays), DOSBox-X
(`tools/Dosbox-X` on Windows, `dosbox-x` on PATH elsewhere, or `U6_DOSBOX`), the toolchain the gate
uses (`tools/turboc20`, `tools/masm300`), and your copy of the game in `gamedata/` (or
`U6_GAMEDATA`) with `CONFIG.U6` set to VGA, mouse and AdLib (the masks are AdLib's), its `U.EXE`, and
its installed `SAVEGAME/OBJLIST` (no character yet: U.EXE refuses to write over one).

## The build

`tools/rpbuild.py`, which Exhume's `replay.py build` runs (`[replay] build` in exhume.toml).
Exhume's own replay build relinks its modding build, which this project does not have, and it
can't express this build: OSILIB's MOUSE and DELAY are swapped for `MOUSERP.ASM` and
`DELAYRP.ASM`, `STACKRP.ASM` and Exhume's `rpfar.asm` are added, all of it assembled by MASM 3.00
and linked by TLINK from a copy of `SRC/u6.lnk`. Every C file is compiled again with `-DREPLAY`;
`replay.c` with `-DRP_FAR_STORAGE`, so that the harness keeps nothing in DGROUP. The headers Turbo C
reads are staged with CRLF line ends.

The build also makes the normal GAME.EXE (which must have the gate's sha1) and compares its map with
the replay build's map. `_DATA`, `_BSS`, `_BSSEND` and every DGROUP public must keep its offset, so
that near pointers kept in the game's state are the real game's. If anything moved, the build fails
and leaves no GAME.EXE.

## Running

Every run copies your game directory (less the GOG release's DOSBox and launcher files,
`[replay] data_skip`) to a scratch C:, puts the replay build there as GAME.EXE, and copies
`build/dos/replay/stage/` over it: `SAVEGAME/OBJLIST` with a character, made by the real U.EXE from
your copy of the game, driven by the key script `[_chargen]` of `tests/replay/sessions.toml` and
checked against its sha1 (`rp.py chargen`, run first by every command that needs it, cached by its
inputs; `tests/replay/README.md` explains why it is the same every time). Every run uses the same DOSBox-X configuration,
`tests/replay/dosbox-x.conf`, and the same start-up batch, so the program loads at the same segment
in all of them:

- **Recording**: a session with `feed` (all of ours) is typed by `tools/keyfeed.py`'s KEYFEED.COM,
  loaded high before GAME.EXE: each key is offered after N of the game's keyboard polls, and the
  clock reads a fixed time (so the seed is fixed). The recording is the same file on every run and
  host, and it runs at the replays' speed. Without `feed`, DOSBox-X's `AUTOTYPE` types the keys in
  real time (cycles=max, turbo off), `wait` seconds after the start and `pace` seconds apart; it
  loses keys now and then and the recording never ends (tests/replay/README.md). Either way F9
  follows the keys. The game's saved game (`[replay] saves`) is copied out too.
- **Recording in js-dos** (Exhume's `replay.py record`, dos-mcp; `npm install` in `exhume/`):
  its recordings replay in DOSBox-X (below, "js-dos and DOSBox-X"). Ours are recorded in
  DOSBox-X with KEYFEED all the same, since a js-dos recording types keys in real time and is a
  different file every time.
- **Replaying**: Exhume's `replaydos.mjs`, DOSBox-X with turbo on. The replay needs no input.
- **Comparing**: checkpoint by checkpoint. A header that differs means the runs went different
  ways. Within a checkpoint, each section is compared byte for byte, apart from the
  `[[replay.mask]]` ranges in exhume.toml (rpgame.h has the reasons; exhume.toml those of the two
  interrupt vectors KEYFEED.COM sets in a recording, int 16h and int 21h, which a replay does not). Two DOS runs are compared
  with `--same-build`, which compares every section. `rp.py compare --cross` is for DOS against
  the port (Phase 2): it skips SEGS and NULL, and takes a stored segment word as equal when it
  names the same block (`[replay.segments]`).
- **Pictures**: the dumps' `SCRN` section (the 320x200 mode 13h frame) with `PAL`. Exhume's
  `replay.py` reads SCRN in place of the VGA planes, which in chain-4 make no picture.

### js-dos and DOSBox-X

Both DOSes load GAME.EXE at the same place and give it the same environment, so a recording
made in either replays in the other, and runs in both hold the same game state.
`tests/replay/dosbox-x.conf`'s `[dos]` settings and exhume.toml's `[replay] dos_env` do this.

Why it was needed: GAME records how much far memory is free (`farcoreleft`, RECORDED). Out of
the box DOSBox-X loaded it 680h paragraphs (26 KB) higher than js-dos, at PSP 812h against
192h. A js-dos recording (5BC18h free) therefore promised a DOSBox-X replay (55418h free) memory
it did not have, and the replay stalled. The 26 KB came from two places:

- **DOSBox-X's padding below 700h.** When `minimum mcb free` is 0, DOSBox-X sets it to 700h. It
  then allocates a DOS-owned block from the end of its kernel tables up to that segment
  (src/dos/dos.cpp: "parity with main DOSBox SVN", against a corruption seen near segment 800h).
  The kernel tables themselves fill 70h..21Ch in conventional memory.
- **The environment.** DOSBox-X's shell has a longer PATH and a PROMPT (62h bytes of strings
  against js-dos's 3Ah), and it gives every program's environment block 1024 bytes of slack
  where mainline DOSBox gives 83. Turbo C's start-up sizes the near heap by the environment
  (`__ENVLNG`, `environ`, `___BRKLVL`), so the size changes DGROUP too.

The settings, checked with a probe that writes its PSP, memory top and environment block in
both DOSes:

| setting | what it does |
|---|---|
| `kernel allocation in umb=true` | DOSBox-X's kernel tables go to upper memory |
| `minimum mcb free=BE` | the padding ends where js-dos's first free block is, so the PSP is 192h |
| `additional environment block size on exec=83` | mainline DOSBox's slack: the environment block is the same size (segment 188h in both) |
| `drive z expand path=false` | `set PATH=Z:\` stays that, not all of Z:'s directories |
| `[replay] dos_env` | PROMPT removed, then PATH, COMSPEC and BLASTER set in that order in the batch before GAME (SET moves a variable to the end, so both DOSes end up with js-dos's block). replaydos.mjs passes it as `--dos-set` to either DOS, and `rp.py record` writes it into its batch. It has to come after the mount: a short PATH in the conf's `[autoexec]` hides DOSBox-X's MOUNT. |

`minimum mcb free` is calibrated to the environment above. If `dos_env` changes, measure the
PSP again.

Result: a js-dos recording replayed in DOSBox-X 2022.12.26 (tools/Dosbox-X) and in
2026.10.01, `first.rec` (DOSBox-X) replayed in js-dos, and `first.rec` replayed under WSL's
snap DOSBox-X. Every section is identical to the recording's own dumps, apart from addresses inside
each emulator's BIOS: the C runtime's saved vectors of int 0, 4, 5 and 6 (DATA 75..81h), a far
pointer at DATA 305Eh, and the vector table in NULL. The saved games are identical. The two
DOSBox-X versions differ from each other in the same way, by BIOS addresses only. No mask
covers those ranges yet: Exhume's mask scopes are `always` and `cross` (DOS against the port),
so `compare --same-build` of a js-dos run against a DOSBox-X run lists them.

### The port against DOS (Phase 2)

The port links the same `replay.c` (exhume/runtime/replay, compiled with the game's sources)
with its own bindings, `port/include/rpport.h`, which `SRC/rpgame.h` includes when the
compiler is not Turbo C. It makes the same hook calls at the same points as the replay DOS
build:
- the key reads, through the shared C's `KEY(a)`;
- the RECORDED values;
- the mouse poll, `rp_mouse` then `rp_mbuttons`, which `port/sys/mouse.c` now makes as
  MOUSERP.ASM does;
- the mouse reset's AX and BX, as RECORDED ids 12 and 13.

`u6 --dos-replay REC --home DIR` replays a recording of the replay DOS build from the saved
game in DIR and writes STATE.OUT there:

    python tools/rp.py port REC OUT          # the port alone (on Windows, its WSL build)
    python tools/rp.py port-check OUT        # first.rec in DOS and in the port, compared

`port-check` compares the dumps across the builds (`replay.py compare` without
`--same-build`: masks of both scopes and `[replay.segments]`). It also compares the saved
games: every file DOS's run changed must be in the port's home, byte for byte.

The port writes GSTA, OBJS, NPCS, SEGS, GDRV, MDRV, FARB, GRMM, RAND, NULL (stand-ins), CNTS,
PAL and SCRN. DATA, BSS and STAK are not written yet, because the port's DGROUP is not one
block in DOS's layout (task 2).

`first.rec` (task 1):
- **Checkpoints:** all 70 headers are the same as DOS's (kind, key, hook calls). From the first
  key on, every section the port writes is identical except GDRV.
- **GDRV:** the graphics driver's image. DOS's driver keeps `struct tGr` and its tables inside
  it, while the port's C driver keeps them in host memory.
- **Checkpoint 0's PAL and SCRN:** taken before the game sets mode 13h. DOSBox-X's text mode
  against the port's black.
- **Saved games:** the five files the session wrote (OBJLIST and four OBJBLK files) are
  identical.
- **Speed:** the port's replay takes about three minutes, because OSI_delay waits in real time,
  as DOS's does.

### The stop key

**F9** (143h) ends a recording. `rpgame.h`'s `RP_STOP_KEY` defines it, and exhume.toml's
`[replay] stop_key` names it. `KBD_GetKey`'s int 16h function 0 never returns F11 or F12, and the
game uses F1 to F8 and F10. Neither the recording nor a replay passes it to the game. `rp.py record`
adds it after the session's keys, and refuses keys that already contain it. A recording that never
reaches it writes no RECORD.OUT.

## Not yet

Goldens and `verify` (Exhume's `replay.py golden`/`verify`, Phase 3); a mask scope for two different DOSes (above);
the port's DATA, BSS and STAK, its graphics driver's state in DOS's layout, and the rest of
Phase 2; music cards other than AdLib, and video other than VGA.
