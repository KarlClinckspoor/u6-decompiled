First release of the native Windows/Linux port of Ultima VI, built from the `portable` branch. It runs the decompiled `GAME.EXE` sources on SDL2.

You need your own copy of Ultima VI (the GOG version works). The port reads the game's files from that folder and never writes there. Start it with `--data <game folder>` once and it remembers the folder. On Windows it also offers a folder picker if it can't find the game.

**What's in it**
- The game itself, with PC-speaker sound effects.
- F11 opens a settings screen over the game (display, sound, game, replays). The game is paused while it's open.
- Character creation for a saved game without a character: name, sex, portrait and the gypsy's virtue choices. It writes the save the same way the original `U.EXE` does.
- Every session is recorded and can be replayed exactly. Recordings made on Windows replay identically on Linux, and the other way round.

**Not yet**
- No music: the AdLib and other sound-card drivers are not ported.
- No intro or main menu (`U.EXE`), and no endgame sequence (`END.EXE`).

**Downloads**
- `u6port-windows-x64.zip`: `u6.exe` with `SDL2.dll`. Unzip and run `u6.exe`.
- `u6port-linux-x64.tar.gz`: `u6`, built on Ubuntu 22.04. It needs glibc 2.35 or newer and SDL2 (`sudo apt install libsdl2-2.0-0`).

`README.md` inside each archive has the options, keys and details.
