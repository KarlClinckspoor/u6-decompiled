The native Windows/Linux port of Ultima VI, now with the original introduction, main menu and character creation, and the AdLib music. It runs the decompiled `GAME.EXE` and `U.EXE` sources on SDL2.

You need your own copy of Ultima VI (the GOG version works). The port reads the game's files from that folder and never writes there. Start it with `--data <game folder>` once and it remembers the folder. On Windows it also offers a folder picker if it can't find the game.

**New in this release**
- `U.EXE`, decompiled: Lord British's title, the bedroom introduction, the main menu, the gypsy who asks her questions and pours the vials, the transfer of an Ultima IV or V character, the acknowledgements, and the moongate into Britannia. It replaces the port's own stand-in character screen.
- Music: the AdLib music in the menu, the introduction and the game, through an emulated OPL2 (Nuked OPL3). Music on/off and its volume are in the settings (F11).
- "Main menu at start" setting (on by default; `--game` skips it when the saved game has a character).

**What's in it**
- The game itself, with its music and PC-speaker sound effects.
- F11 opens a settings screen over the game or the menu (display, sound, game, replays). The game is paused while it's open.
- Every game session is recorded and can be replayed exactly. Recordings made on Windows replay identically on Linux, and the other way round. (The part in the main menu is not recorded.)

**Not yet**
- The endgame sequence (`END.EXE`).
- Sound cards other than the AdLib, and video modes other than VGA.

**Downloads**
- `u6port-windows-x64.zip`: `u6.exe` with `SDL2.dll`. Unzip and run `u6.exe`.
- `u6port-linux-x64.tar.gz`: `u6`, built on Ubuntu 22.04. It needs glibc 2.35 or newer and SDL2 (`sudo apt install libsdl2-2.0-0`).

`README.md` inside each archive has the options, keys and details. The OPL emulator is Nuked OPL3 (LGPL 2.1, its license is included).
