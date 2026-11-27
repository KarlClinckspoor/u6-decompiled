# Ultima VI, native port (branch `portable`)

The game's own C compiled for Windows and Linux on SDL2: `GAME.EXE`'s (`SRC/`) and `U.EXE`'s
(`SRC_U/`: the title, the main menu, the introduction, the gypsy's character creation, the
transfer of a character) and `END.EXE`'s (`SRC_END/`: the ending), the same files that still build byte-identical programs for DOS,
with the DOS pieces they need rewritten in C under `port/`: the OSI assembly libraries,
the MCGA graphics drivers, the AdLib music driver, DOS files and memory. On top: a settings
screen over the game (F11) and a recording of every session that replays exactly.

You need your own copy of Ultima VI (the GOG version works): the port reads the game's data
files from its folder and never writes there.

## Building

Ready-made builds for Windows and Linux are on the repository's Releases page.

The port lives on the `portable` branch, so check it out first (the commands below run
from the repository's top folder):

    git fetch origin
    git checkout portable

Linux, and Windows' WSL (Debian/Ubuntu: `sudo apt install cmake ninja-build clang libsdl2-dev`;
under WSL the window opens through WSLg, Windows 11 or a recent Windows 10):

    cmake -S port -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/u6 --data "/path/to/Ultima 6"

Under WSL the game folder can stay on the Windows side, e.g.
`--data "/mnt/c/GOG Games/Ultima 6"`.

Windows, with MSYS2 (UCRT64 shell: `pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2}`):

    cmake -S port -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

then copy `SDL2.dll` (from the MSYS2 `ucrt64/bin` folder) next to `build/u6.exe`.

Windows, cross-compiled from Linux (`apt install mingw-w64`, and SDL2's
`SDL2-devel-2.x-mingw.tar.gz` from its GitHub releases):

    cmake -S port -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_TOOLCHAIN_FILE=$PWD/port/cmake/mingw64.cmake \
          -DSDL2_DIR=/path/to/SDL2-2.x/x86_64-w64-mingw32/lib/cmake/SDL2
    cmake --build build-win

and put `SDL2.dll` (from `SDL2-2.x/x86_64-w64-mingw32/bin`) next to `u6.exe`.
MSVC's own compiler does not take the game's C89; clang-cl does.

The DOS build is unchanged: `python3 tools/dosbuild.py --tools DIR` builds `GAME.EXE` with
Turbo C 2.0 and MASM in a headless DOSBox and checks that it is identical to the reference.

## Playing

`u6 --data FOLDER` the first time (the folder with `MAPTILES.VGA`, `U6PAL`, ...); it is
remembered. Without `--data` the port also looks in the current folder, `U6_DATA`, and the
usual GOG folders.

- **Home folder**: settings (`u6port.cfg`), your saved game (`SAVEGAME/`), recordings and
  backups. `%APPDATA%\u6port` on Windows, `~/.local/share/u6port` on Linux; `--home DIR`
  chooses another. The game's writes go here; files it has not written are read from the
  game folder.
- **The main menu**: as `ULTIMA6.EXE` does in DOS, the port starts with `U.EXE`: Lord
  British's title and the bedroom, then the main menu (Introduction, Create a Character,
  Transfer a Character from Ultima IV or V, Acknowledgements, Journey Onward). A new
  character goes on with the introduction, the moongate and the game; Journey Onward
  goes to the game. With the setting "Main menu at start" off (or `--game`), the port goes
  straight to the game when the saved game has a character. A saved game without one
  always starts with the menu, as `GAME.EXE` sends you back there in DOS.
- **The ending**: when the lenses are placed on the Codex, `GAME.EXE` runs `END.EXE` with
  the time spent in Britannia; the port plays it in its place (the kings and the lenses,
  the Codex among the stars, the sixteen pages of the text, the congratulations), and
  ends there as DOS did. `--ending Y,M,D` plays it on its own.
  To transfer a character, put Ultima IV's `PARTY.SAV` or Ultima V's `SAVED.GAM` in the
  home folder and choose drive C (the port's only drive).
- **Music**: the AdLib music, in the menu and the introduction and in the game
  (`port/sys/adlib.c`: the U6ADLIB.DRV driver in C on an emulated OPL2, Nuked OPL3).
- **F11**: the settings screen; the game is paused while it is open.
  - Display: fullscreen (also Alt+Enter), window scale, 4:3 aspect, whole-number scaling.
  - Sound: the PC speaker effects and their volume; the music and its volume.
  - Game: the game folder, recording on/off, the main menu at start, show the screen at
    start, New game (your
    saved game moves to `backups/` and the game folder's own, as installed, comes back).
  - Replay: the recordings, to watch; while one plays, its speed and "take over".
  - Fixes and Enhance: each fix and each enhancement on or off, with a row that turns them
    all on or off, and the selected one's description underneath. The choice is saved as
    `fixes=` and `enhance=` and applies at the next start, because a session keeps its set
    (its recording carries it). A row whose saved choice differs from the running session's
    says "On, next start" or "Off, next start".
- Saving and loading are the game's own: Ctrl+S saves (it asks first), Ctrl+R goes back to
  the last save, Alt+X or Ctrl+Q quits. Neither quitting nor closing the window saves.

## Fixes of the original's faults

The port can fix the faults the code review rated critical (docs/CODE_REVIEW.md), all at once:
`--fixes all`, `fixes=all` in `u6port.cfg`, or the settings screen's Fixes tab (F11). They are
**off by default**, and off the port plays exactly as DOS.

On, they correct nine faults:
- the vortex cube ending the game for eight potions;
- the inventory hang;
- the spell cast at a map target;
- the powder keg blowing up whatever takes its place;
- the healer raising the wrong body;
- the lens beams drawn outside the view;
- objects lost when an area is too full;
- the fountain's empty wish;
- the object order where the map wraps.

Enhancements, which change what the original does by design, are turned on apart from the fixes:
`--enhance NAMES` or `all`, `enhance=` in `u6port.cfg` (`--enhance list`), or the Enhance tab (F11):
- `moonstone-guard` asks before a moonstone is buried;
- `grouped-finds` lists identical found items once ("throwing axes (7x)");
- `look-tile` makes Look list everything on a tile;
- `choose-get` lets the arrow keys choose what Get takes.

A session played with fixes is recorded with them and replays with them. What each one does,
how they are built (port-only lines in SRC, so GAME.EXE stays identical) and how they are tested:
[docs/port/FIXES.md](FIXES.md); `u6 --fixes list`.

## Recordings and replays

Every session is recorded (unless turned off), the black box: `recordings/YYYYMMDD-HHMMSS/`
in the home folder holds `RECORD.OUT` and `stage/`, a copy of the saved game the session
started from. The newest five are kept. The game's exit, closing the window and a crash all
close the recording, so a crash can be replayed up to the moment it happened. A replay copies
`stage/` into a temporary folder (your own save is never touched) and feeds the game the
recorded inputs; the port exits at the end of the recording, unless you take over (F11,
Replay) and play on from there.

The recorder is Exhume's `runtime/replay/replay.c`, the same code the replay DOS build runs
(docs/replay.md), so a port recording and a DOS recording are in the same format. It records
every value the game's own code reads from outside: the keys, the mouse, the random seed, and
the few machine values the game asks DOS and the BIOS for (the DOS version, the drives, the
memory). The timer ticks are not among them: they only pace the game, so a replay runs on a
virtual clock, as fast as you ask. Because the game's C is compiled as Turbo C saw it (16-bit
`int`, packed structs, Borland's random generator, the DOS memory layout where the game
depends on it), a recording made on Windows replays identically on Linux and the other way
round.

    u6 --replay 20261025-093612           # a recording in the home folder, at its own pace
    u6 --replay DIR --speed 0 --hidden    # as fast as possible, no window
    u6 --replay DIR --out OUT             # run it in OUT, writing its dumps (STATE.OUT) there
    u6 --record DIR --keys "..."          # a test session, with dumps; F9 ends it
    u6 --replay DIR --speed 0 --hidden --video bug.mp4   # a recording as a video

A player who finds a bug can send the recording folder (`RECORD.OUT` and `stage/`). Replayed
with `--video`, it becomes a video at the game's real pace, whatever the replay's speed: at
`--speed 0` it takes seconds, and it can also be made while watching it in the window at 2, 4
or 8. The frames are taken when the replay's clock advances, at the same point of the game every
run, so the same recording always gives the same video. They show the game's screen without the
port's overlay. ffmpeg must be installed (or named by `U6_FFMPEG`). A recording made with fixes
or enhancements replays and films with them.

A `--record` session also writes the state dumps (`STATE.OUT`, docs/replay.md) at every key,
mouse button change and at its end; its replay with `--out` writes the same dumps, and
`tools/replay.py compare` (or `python tools/rp.py port-self OUT`) compares them.

## Options

    --data DIR          the game folder (remembered)
    --home DIR          where settings, saved games and recordings go
    --replay REC        play a recording (a folder, or a name in recordings/)
    --out DIR           with --replay: run it in DIR and write its dumps there (tests)
    --speed N           replay speed: 1, 2, 4, 8, or 0 for as fast as possible
    --record DIR        record this session into DIR, with dumps; F9 ends it (tests)
    --dos-replay FILE   replay a RECORD.OUT (the replay DOS build's or the port's) from the
                        saved game in --home, writing STATE.OUT there (docs/replay.md)
    --no-recording      no black box recording this time
    --hidden            no window (replays, tests)
    --settings          open the settings screen before the game
    --scale N, --fullscreen, --no-aspect, --no-integer, --no-audio
    --menu, --game      start with the main menu (U.EXE), or with the game
    --new-avatar SPEC   make the character without the gypsy (tests): NAME[,M|F[,PORTRAIT[,VIRTUE]]]
    --ending Y,M,D      play the ending (END.EXE), as after Y years, M months and D days
    --screenshot FILE   write the screen as a BMP when the session ends (tests)
    --run-ms N          end the session after N ms (tests)
    --fixes LIST        fix the original's faults: all, none, or names (--fixes list;
                        docs/port/FIXES.md); off by default. With --dos-replay (tests) a DOS
                        recording is replayed with them
    --enhance LIST      changes to the original's design: all, none, or names (--enhance
                        list: moonstone-guard, which asks before burying a moonstone); off by
                        default
    --keys SCRIPT       type keys at given times (tests): "1500:up 2000:a 2500:^s"; each
                        key waits until the game has taken the one before. The mouse too, in
                        the game's pixels: MS:mouse:X,Y moves it, MS:click:X,Y and
                        MS:rclick:X,Y click (a mouse is there even with --hidden)
    --farcore N         farcoreleft() at most N (tests: the replay DOS build's figure, so that
                        a session recorded in the port replays in DOS)
    --feed SCRIPT       keys typed as KEYFEED.COM types them in DOS ("GAP:AX ...", counted in
                        the game's keyboard polls), with DOSBox-X's mouse and drive Z:. With
                        --seed and --cpuspeed (DOS's), a session recorded in the port is the
                        DOS recording, checkpoint for checkpoint (tests: rp.py port-record of a
                        `feed` session); with an enhancement on it differs only where that acts
    --seed N, --cpuspeed N   the random seed and the delay calibration (tests: DOS's)
    --video FILE        the session as a video (an .mp4 through ffmpeg, or U6_FFMPEG), one
                        frame per tick of the game's clock (18.2 a second), 640x400, without the
                        overlay: a replay's plays at the game's own pace however fast it ran, and
                        is the same at any --speed. It only reads the screen

## How it is put together

- `SRC/u6port.h` (over Exhume's `portable.h`, from the `exhume/` submodule): the only kind of change to the shared sources. Explicit widths
  (`int16`, `uint32`, ...) and a few macros (varargs, a pointer kept in an integer, the
  music driver's far call, a string literal the game writes into) that expand to exactly
  the original tokens under Turbo C. `SRC/u6.h` includes the port's prototypes on the host
  only.
- `port/include/compat.h`, force-included in the game's files on the host: the memory-model
  keywords go away, `MK_FP`/`FP_SEG`/`FP_OFF` and the far heap work on a 1 MB DOS arena
  (`port/sys/dosmem.c`), Borland's library names are provided, every struct is packed.
- Data layout: the game's uninitialised data is one block in DOS's order
  (`port/sys/bss.c`, from `port/tools/gen_bss.py`), and two files keep DOS's exact layout as
  packed blocks (`port/tools/gen_block.py`): `gamestat.c` (the saved game's state block)
  and `monsdata.c` (read past its tables for creatures that are not monsters).
- `port/sys/`: the OSI library (files, LZW, random, timer, keyboard, mouse, speaker) and
  the DOS file system and interrupts, in C (`docs/port/OSILIB.md`); `portmain.c` (start-up)
  and `session.c` (home folder, recordings, replays).
- `port/gfx/mcga.c`: the MCGA driver U6MCGA.DRV in C (`docs/port/MCGA_DRIVER.md`).
- `port/u/`: `U.EXE` in the port. Its C (`SRC_U/`, with `SRC_U/u6port.h` as `SRC/` has)
  is compiled with its public names renamed `U_*` (`uport.h`, `urename.h`), so that both
  programs live in one executable; `uprot.h` (prototypes) and `ubss.c` (its uninitialised
  data in DOS's order) come from `port/tools/gen_u.py`. `ugfx.c` is its graphics driver
  MCGADRV.BIN in C (`docs/port/U_DRIVER.md`), `osilib/` its OSI library in C
  (`docs/port/U_OSILIB.md`), `urun.c` the rest (LoadZFile, the DOS handlers, the code its
  noise effects read). `U.EXE`'s exec of `GAME.EXE` goes on with the game in the same
  process; `GAME.EXE`'s exec of `ULTIMA6.EXE` restarts the port with the menu.
- `port/end/`: `END.EXE` in the port, the same way: its C (`SRC_END/`) with its names
  renamed `E_*` (`eport.h`, and `erename.h`, `eprot.h`, `ebss.c` from
  `port/tools/gen_u.py --prog end`). It links the same OSI library, LoadZFile and graphics
  driver as `U.EXE`, so it uses their ports in `port/u/` (the library's names stay `U_*`);
  `lib/stars.c` is its starfield (`stars.asm`) in C, `erun.c` runs it. `GAME.EXE`'s exec of
  `END.EXE` runs it in the same process, after the session (the ending is not recorded).
- `port/sys/adlib.c`: the AdLib driver U6ADLIB.DRV in C, run at 60 Hz in the audio
  callback, through Nuked OPL3 (`port/third_party/nuked-opl3`, LGPL 2.1).
- Recording and replaying: Exhume's `runtime/replay/replay.c`, with the port's bindings
  (`port/include/rpport.h`, `port/game/rpport.c`); `port/ui/`: the settings screen, the
  settings file; `port/platform/`: `plat.h` and its SDL2 backend.
- Not ported: the other music drivers (Roland, CMS, Tandy, ...: the port plays AdLib), the
  graphics modes other than VGA. The parts in `U.EXE` and `END.EXE` are not recorded; the
  recording is the game's.

## Tests

- `port/tests/run_osilib_tests.sh`: the C OSI library against the original routines of
  `GAME.EXE` run in unicorn (`pip install unicorn`), and unit tests.
- `python3 port/tests/mcga_diff.py`: the C MCGA driver against U6MCGA.DRV in unicorn.
- `python3 port/tests/ugfx_diff.py`, `python3 port/tests/uosi_diff.py`: `U.EXE`'s graphics
  driver and OSI library against MCGADRV.BIN and `U.EXE`'s own routines in unicorn.
- `python3 port/tests/adlib_diff.py`: the AdLib driver's OPL writes against U6ADLIB.DRV's,
  every song.
- Replays: `python tools/rp.py port-self OUT [--all]` records a session with `--record` and
  `--keys`, replays it with `--out`, and compares the dumps and the saved games; then the same
  keys as a black box session, whose replay must leave the same saved game.
  `python tools/rp.py port-check OUT --all` replays DOS's recordings in the port
  (docs/replay.md); with `--fixes all`, with the fixes on (docs/port/FIXES.md); with `--video
  DIR`, also a video of each build's replay (REC-VARIANT.mp4; `build/videos` is outside git).
  Since the port replays DOS's recordings identically, the video is DOS's run. `rp.py port REC
  OUT --session S --video FILE` makes one alone.
  `python tools/rp.py port-record OUT SESSION [--fixes LIST]` records a session in the port
  from its stage: its `port_keys` (the mouse) or its `keys`; without fixes the recording
  replays in DOS (`rp.py check OUT2 --session SESSION --rec OUT/rec/RECORD.OUT`).
