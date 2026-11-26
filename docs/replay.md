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
| `python tools/rp.py record OUT [--session NAME \| --keys "..."]` | record a session (default: `smoke`) |
| `python tools/rp.py replay OUT/RECORD.OUT OUT2` | replay it |
| `python tools/rp.py compare OUT OUT2` | compare two dumps, masks applied |
| `python tools/rp.py show OUT2 PNGDIR` | list the dumps, write each one's screen as a PNG |
| `python tools/rp.py log OUT/RECORD.OUT` | what a recording holds |
| `make replay-check` or `python tools/rp.py check OUT` | all of it: record, replay twice, compare the recording with the first replay and the two replays, PNGs of the second |

`rp.py` is a front end. Building, replaying, comparing, showing and the log are Exhume's
`exhume/tools/replay.py` with `--config exhume.toml`; recording is `rp.py`'s own.

Needs: Python 3.11+, Node.js (Exhume's `replaydos.mjs` runs the replays), DOSBox-X
(`tools/Dosbox-X` on Windows, `dosbox-x` on PATH elsewhere, or `U6_DOSBOX`), the toolchain the gate
uses (`tools/turboc20`, `tools/masm300`), and your copy of the game in `gamedata/` (or
`U6_GAMEDATA`) with `CONFIG.U6` set to VGA, mouse and AdLib (the masks are AdLib's).

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
`tests/replay/stage/` over it. That folder holds a saved game with a character
(`tests/replay/README.md` says how it was made). Every run uses the same DOSBox-X configuration,
`tests/replay/dosbox-x.conf`, and the same start-up batch, so the program loads at the same segment
in all of them:

- **Recording**: DOSBox-X's `AUTOTYPE` types the session's keys in real time (cycles=max, turbo off),
  `wait` seconds after the start and `pace` seconds apart (`tests/replay/sessions.toml`). Then it
  sends **F9**. Exhume records in js-dos through dos-mcp instead; that comes with Phase 1 task 8.
- **Replaying**: Exhume's `replaydos.mjs`, DOSBox-X with turbo on. The replay needs no input.
- **Comparing**: checkpoint by checkpoint. A header that differs means the runs went different
  ways. Within a checkpoint, each section is compared byte for byte, apart from the
  `[[replay.mask]]` ranges in exhume.toml (rpgame.h has the reasons). Two DOS runs are compared
  with `--same-build`, which compares every section. `rp.py compare --cross` is for DOS against
  the port (Phase 2): it skips SEGS and NULL, and takes a stored segment word as equal when it
  names the same block (`[replay.segments]`).
- **Pictures**: the dumps' `SCRN` section (the 320x200 mode 13h frame) with `PAL`. Exhume's
  `replay.py` reads SCRN in place of the VGA planes, which in chain-4 make no picture.

Compare runs made by one DOSBox-X install. A replay under WSL's snap DOSBox-X of a recording made
with Windows' `tools/Dosbox-X` went the same way at every checkpoint, but a few bytes differed:
DATA 78h, 88h..8Ah and 9Eh (C0's variables), interrupt vectors in NULL, STACKRP's start in STAK, and one
BSS byte.

### The stop key

**F9** (143h) ends a recording. `rpgame.h`'s `RP_STOP_KEY` defines it, and exhume.toml's
`[replay] stop_key` names it. `KBD_GetKey`'s int 16h function 0 never returns F11 or F12, and the
game uses F1 to F8 and F10. Neither the recording nor a replay passes it to the game. `rp.py record`
adds it after the session's keys, and refuses keys that already contain it. A recording that never
reaches it writes no RECORD.OUT.

## Not yet

Goldens and `verify` (Exhume's `replay.py golden`/`verify`, Phase 3); recording in js-dos (task 8);
the port linking `replay.c` and replaying DOS's recordings (Phase 2), which is when
`[replay.segments]` gets its first real use; music cards other than AdLib, and video other than VGA.
