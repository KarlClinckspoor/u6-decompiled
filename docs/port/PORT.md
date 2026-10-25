# Ultima VI, native port (branch `portable`)

The game's own C compiled for Windows and Linux on SDL2: `GAME.EXE`'s (`SRC/`) and `U.EXE`'s
(`SRC_U/`: the title, the main menu, the introduction, the gypsy's character creation, the
transfer of a character), the same files that still build byte-identical programs for DOS,
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
- Saving and loading are the game's own: Ctrl+S saves (it asks first), Ctrl+R goes back to
  the last save, Alt+X or Ctrl+Q quits. Neither quitting nor closing the window saves.

## Recordings and replays

Every session is recorded (unless turned off): `recordings/YYYYMMDD-HHMMSS/` in the home
folder holds `RECORD.U6R` and `stage/`, a copy of the saved game the session started from.
The newest five are kept. A replay copies `stage/` into a temporary folder (your own save is
never touched) and feeds the game the recorded inputs.

What is recorded is every value the game reads from outside: the timer ticks, the keys and
the shift state, the mouse, the random seed, the date and time. Each is a stream of
(value, repeat count) runs. Every 4096 reads the recording also stores a digest of the
game's state (objects, NPCs, party, clock, map position, the random generator), and the
replay compares it: a replay that went another way says so (and `u6 --replay` exits with
code 3). Because the game's C is compiled as Turbo C saw it (16-bit `int`, packed structs,
Borland's random generator, the DOS memory layout where the game depends on it), a
recording made on Windows replays identically on Linux and the other way round.

    u6 --replay 20261025-093612           # a recording in the home folder, at its own pace
    u6 --replay DIR --speed 0 --hidden    # as fast as possible, no window
    u6 --replay DIR --continue            # play on from where the recording ends

## Options

    --data DIR          the game folder (remembered)
    --home DIR          where settings, saved games and recordings go
    --replay REC        play a recording (a folder, or a name in recordings/)
    --speed N           replay speed: 1, 2, 4, 8, or 0 for as fast as possible
    --continue          after a replay ends, keep playing (default: quit)
    --record DIR        record this session into DIR
    --no-recording      no black box recording this time
    --hidden            no window (replays, tests)
    --settings          open the settings screen before the game
    --scale N, --fullscreen, --no-aspect, --no-integer, --no-audio
    --menu, --game      start with the main menu (U.EXE), or with the game
    --new-avatar SPEC   make the character without the gypsy (tests): NAME[,M|F[,PORTRAIT[,VIRTUE]]]
    --screenshot FILE   write the screen as a BMP when the session ends (tests)
    --run-ms N          end the session after N ms (tests)
    --keys SCRIPT       type keys at given times (tests): "1500:up 2000:a 2500:^s"

## How it is put together

- `SRC/portable.h`: the only kind of change to the shared sources. Explicit widths
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
- `port/u/`: `U.EXE` in the port. Its C (`SRC_U/`, with `SRC_U/portable.h` as `SRC/` has)
  is compiled with its public names renamed `U_*` (`uport.h`, `urename.h`), so that both
  programs live in one executable; `uprot.h` (prototypes) and `ubss.c` (its uninitialised
  data in DOS's order) come from `port/tools/gen_u.py`. `ugfx.c` is its graphics driver
  MCGADRV.BIN in C (`docs/port/U_DRIVER.md`), `osilib/` its OSI library in C
  (`docs/port/U_OSILIB.md`), `urun.c` the rest (LoadZFile, the DOS handlers, the code its
  noise effects read). `U.EXE`'s exec of `GAME.EXE` goes on with the game in the same
  process; `GAME.EXE`'s exec of `ULTIMA6.EXE` restarts the port with the menu.
- `port/sys/adlib.c`: the AdLib driver U6ADLIB.DRV in C, run at 60 Hz in the audio
  callback, through Nuked OPL3 (`port/third_party/nuked-opl3`, LGPL 2.1).
- `port/replay/`: the recording format; `port/ui/`: the settings screen, the settings
  file; `port/platform/`: `plat.h` and its SDL2 backend.
- Not ported: the other music drivers (Roland, CMS, Tandy, ...: the port plays AdLib), the
  graphics modes other than VGA, `END.EXE` (the endgame). The part of the session in
  `U.EXE` is not recorded; the recording starts with the game.

## Tests

- `port/tests/run_osilib_tests.sh`: the C OSI library against the original routines of
  `GAME.EXE` run in unicorn (`pip install unicorn`), and unit tests.
- `python3 port/tests/mcga_diff.py`: the C MCGA driver against U6MCGA.DRV in unicorn.
- `python3 port/tests/ugfx_diff.py`, `python3 port/tests/uosi_diff.py`: `U.EXE`'s graphics
  driver and OSI library against MCGADRV.BIN and `U.EXE`'s own routines in unicorn.
- `python3 port/tests/adlib_diff.py`: the AdLib driver's OPL writes against U6ADLIB.DRV's,
  every song.
- Replays: a recording made with `--keys` and replayed with `--hidden --speed 0` must report
  "all identical".
