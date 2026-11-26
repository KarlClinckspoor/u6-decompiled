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
| `python tools/rp.py chargen [OUT]` | the character every run starts from: U.EXE run by script (cached in `build/dos/stage/`) |
| `python tools/rp.py record OUT [--session NAME \| --keys "..." [--feed N]]` | record a session (default: `smoke`) |
| `python tools/rp.py replay OUT/RECORD.OUT OUT2` | replay it |
| `python tools/rp.py stage NAME` | the saved game session NAME starts from: the character with its `setup` (below, "Set-ups through the saved game") |
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
`build/dos/stage/` over it: `SAVEGAME/OBJLIST` with a character, made by the real U.EXE from
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

    python tools/rp.py port REC OUT [--variant V]       # the port alone
    python tools/rp.py port-check OUT [--rec REC] --all  # REC (default first.rec) in DOS and in every build

`port-check` compares the dumps across the builds (`replay.py compare` without
`--same-build`: masks of both scopes and `[replay.segments]`). It also compares the saved
games: every file DOS's run changed must be in the port's home, byte for byte.

The port's builds (`--variant`, or `--all` for the four):

| variant | built with | where |
|---|---|---|
| `linux-gcc` (default) | gcc | on Windows in WSL (`~/u6port-replay`), else `build/port-linux-gcc` |
| `linux-clang` | clang | WSL `~/u6port-replay-clang`, else `build/port-linux-clang` |
| `win-gcc` | MSYS2 MINGW64's gcc and SDL2 | `build/port-win-gcc` |
| `win-clang` | MSYS2 CLANG64's clang, MINGW64's SDL2 | `build/port-win-clang` |

`U6_MSYS2` names MSYS2's folder (default `C:\msys64`). A Windows build runs with MINGW64's `bin`
on PATH, for SDL2.dll. `U6_PORT` still names an executable that is used as it is.

A `--dos-replay` run takes about a second. Its clock is a virtual one that OSI_delay's waits
advance (`port/sys/timer.c`): nothing the game decides on reads the tick count, which the DOS
build keeps outside DGROUP (DELAYRP.ASM). It reads no live input, and the busy delay loops
return at once.

The port writes every section DOS does.

- **DATA and BSS** are DGROUP in DOS's layout. `port/tools/gen_dgroup.py` (rerun with
  `python tools/rp.py port-dgroup`) generates `port/game/dgroup_port.c`, a table of DOS byte
  ranges and where the port keeps each one:
  - BSS.ASM's objects (`port/sys/bss.c`);
  - the DATA publics of RGAME.MAP, matched to the port's symbols by name;
  - the statics the sources give a DOS offset for, reached through generated wrappers
    (`port/game/tu_NAME.c`, `dgs_NAME.inc`);
  - the writable literals (`WRITABLE_STR`).

  `u6_rp_dgroup` (`port/game/rpport.c`) writes the image from it. Each pointer is written in
  DOS's form:
  - a far pointer as the arena's segment:offset, or as DGROUP:offset;
  - a near pointer as its DOS offset;
  - a code pointer as RGAME.MAP's segment:offset, counted from DGROUP's paragraph;
  - the drivers' entry points as the last entry called.
- **GDRV** holds the graphics driver's segment as DOS's driver keeps it: `mcga.c`'s `drv[]` at
  its DOS offsets, and the tGr block in DOS's layout at CS:0052.
- **STAK** is zeros; the stack measure is DOS's.

What the port has no counterpart for is masked, `scope = "cross"`, each with its reason:
- Turbo C's run-time data, the game's string literals, the library's data and BSS;
- INFLATE.ASM's work variables;
- near pointers to string literals;
- the file handles that the replay build's own files shift;
- the DOS driver's mouse temporaries.

`gen_dgroup.py --masks` writes the generated ones into exhume.toml.

The first dump comes before the level exists (MOUSE_INIT's first button read):
- **Segment words.** DOS writes SEGS only from the level on. A dump without SEGS is compared
  with each run's first SEGS, by the load segment's rule and `heap_block`'s only: neither moves
  during a run (Exhume's `replay.py`, `segment_words` with `rules_only`).
- **Text mode.** DOS's VGA is still in text mode 3 there. The port writes what that mode holds
  (`u6_rp_screen`, port/game/rpport.c): the DAC the VGA BIOS loads for mode 3 (the EGA's 64
  colours, the rest 0), and FFh for A000:0000, which nothing answers in text mode. In mode 13h,
  from GR_INIT to GR_SHUTDOWN (`mcga_mode13`), it writes its own DAC and screen.
- **Mouse.onMove.** It is the DOS driver's default handler, `_MOUSE_0055`. The port's carries
  that name, so the table finds it.

**Task 3 result:** `first.rec` and `smoke` are identical at every checkpoint, in every section
apart from the masks, in all four builds. The saved games are identical too.

The masks that cover bytes DOS changes during the session:
- OSI_ErrorFile;
- VarStr (but its 'N' slot);
- the driver's mouse temporaries;
- INFLATE.ASM's variables;
- the near heap's pointers.

Task 1's result, before DATA and BSS were written:
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

### The port's own recordings (Phase 2 task 4)

The port records and replays through the same `replay.c`; its old recorder (`port/replay`, a
digest of the game state every 4096 reads) is gone. `port/sys/session.c` sets replay.c's mode
before the game starts:

| | what replay.c does | files |
|---|---|---|
| the black box (every player's session) | records, no dumps, F9 passes to the game; closed at exit, when the window closes and at a fault | `recordings/YYYYMMDD-HHMMSS/RECORD.OUT` and `stage/` in the home folder |
| `--record DIR` (tests) | records with the dumps; F9 ends it | `DIR/RECORD.OUT`, `DIR/STATE.OUT`, `DIR/stage/` |
| `--replay REC` (the player, or the settings screen) | replays in a temporary home, no dumps; the port exits at the end, or the player takes over | |
| `--replay REC --out OUT` (tests) | replays in OUT, with the dumps | `OUT/STATE.OUT`, `OUT/SAVEGAME/` |
| `--dos-replay FILE` | replays FILE from the saved game in `--home`, with the dumps | `STATE.OUT` in the home |

A replay that only shows a recording sets replay.c's `rp_blackbox`, which also stops it writing
STATE.OUT. Taking over is Exhume's `rp_take_over` (port only): between two hook calls the replay
ends, and the hooks read the world from then on. A replay runs on the virtual clock above, paced
to `--speed` (or the settings screen's), so the ticks need no stream.

    python tools/rp.py port-self OUT [--variant V ... | --all]

records a session with `--record` and a key script, replays it with `--out`, and compares the
two runs' dumps (`replay.py compare --same-build`) and saved games. Then it plays the same keys
as a black box session and checks that its replay leaves the same saved game. The port's
`--keys` gives each key once the game has taken the one before, so no key is lost while the game
is busy (`KBD_GetKey(1)` keeps only the last key in the buffer).

**Task 4 result:** port-self is identical in all four builds: 14 checkpoints, and the saved game
(OBJLIST and four OBJBLK files) the same after the replay, for both kinds of session. `first.rec`
and `smoke` are still identical to DOS in all four builds.

### Set-ups through the saved game

A session that needs another starting point (a place, items, attributes, a quest's state) says
so in its `setup` in tests/replay/sessions.toml: `tools/u6save.py` commands, which
`rp.py`'s `session_stage` applies to the character's saved game, in `build/dos/stage-NAME`. DOS's
recording and replays and the port's replays all start from that stage, so the session is
still proved against DOS. Runtime cheats (Exhume's, port-only) would not replay in DOS.

    python tools/rp.py stage NAME                        # make it and show it
    python tools/rp.py check OUT --session NAME          # record in DOS, replay twice
    python tools/rp.py port-check OUT --rec OUT/rec/RECORD.OUT --session NAME --all
    python tools/u6save.py DIR "show"                    # any saved game

The commands:
- `show`;
- `set NAME VALUE`: a field of the state block, with SRC/gamestat.c's names (KARMA, Time_H,
  IsOnQuest, MoonstonePos[3] ...);
- `npc WHO FIELD VALUE`: str, dex, int, magic, hp, exp, level, movepts, status, talkflags,
  mode, commode, flag, schedindex, or name for a party member;
- `teleport X Y Z`: the party, with their belongings;
- `give WHO TYPE [QUANTITY [QUALITY [FRAME]]]`.

WHO is an NPC number, `avatar`, or a party member's name.

The editor writes the game's own formats:
- **OBJLIST** as GAME_Save writes it: the NPCs' objects and tables, then the state block, 1C73h
  bytes.
- **The area files** (OBJBLKxy) as OBJ_UnloadArea writes them. An item an NPC holds is stored
  in the file of the area where the NPC stands, so a teleport moves the party's items to the
  new area's file. There they go where the game's order puts them (OBJ_CompareOrder: by the
  owner's tile, y then x). Items in containers keep their containers, as indexes in the file.

Every area file of the game reads and writes back byte for byte, including the third position
byte of a contained item, which the game never sets (its stack junk).

The teleport puts every party member on the one tile, as the game's own PartyExit does before
the followers step out. It refuses when the party is in a vehicle.

### The stop key

**F9** (143h) ends a recording. `rpgame.h`'s `RP_STOP_KEY` defines it, and exhume.toml's
`[replay] stop_key` names it. `KBD_GetKey`'s int 16h function 0 never returns F11 or F12, and the
game uses F1 to F8 and F10. Neither the recording nor a replay passes it to the game. `rp.py record`
adds it after the session's keys, and refuses keys that already contain it. A recording that never
reaches it writes no RECORD.OUT.

## Not yet

Goldens and `verify` (Exhume's `replay.py golden`/`verify`, Phase 3); a mask scope for two
different DOSes (above); music cards other than AdLib, and video other than VGA.
