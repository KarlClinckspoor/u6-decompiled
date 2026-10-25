# u6-decompiled
Decompiled code for PC/MSDOS version of Ultima VI - The False Prophet (version 4.5)


This project allows you to build `GAME.EXE`, with very few differences from the original.
I have also decompiled most of the other executables, but I have to make the code more readable before releasing it.

The techniques used to decompile `GAME.EXE` is basically the same I used to [decompile Ultima IV](https://github.com/ergonomy-joe/u4-decompiled):
* disassemble the executable
* gather the tools/libraries that were originaly used (Borland Turbo C 2.0 in our case)
* search for modules/functions separation in the code.
* for each function, read assembly code and try to guess what was the original C code
* build new code and compare the resulting binary with original binary
* eventually correct code and build again
* repeat 3 last steps until all the executable file has been reversed

Some years ago, someone released **Ultima 6 Technical Documents** online, which proved very helpful to understand some parts of the code, and to give a name to some functions/variables/macros.
Be aware though that these documents refer to an earlier version of the game, and so they do not reflect exactly what is in the code.


Concerning the build process:  
The original game was build with Borland Turbo C 2.0 for the part written in C language (most of the files).
There are some `asm` files too, you will need tasm 2.0 and masm 3.0 to build them.

All you need to know for the build process is in the file `SRC/doit.bat` which uses `MAKEFILE`.
Before that, you will need to launch `SRC/OSILIB/doit.bat`. I think I put most of the `asm` files in there.

The naming of the C files is not friendly at all, and I apologize for that. Maybe some renaming will take place in the future ?

Have a lot of decompiling fun!  
2022/03/29 ergonomy_joe

PS: when I first decompiled this code, I posted some comments on a [french abandonware related forum](https://www.abandonware-forums.org/), you can take a look [here](https://www.abandonware-forums.org/forum/forum-ltf-abandonware-france/le-bar-des-amis/36760-ultima-vi-les-codes-sources-r%C3%A9g%C3%A9n%C3%A9r%C3%A9s?35874-Ultima-VI-les-codes-sources-r%E9g%E9n%E9r%E9s=).

---

## Readable version (branch `readable`)

On this branch the C source has been rewritten for readability **without changing the generated code**:
the source should still build a `GAME.EXE` identical to the one built from `main`.

What changed:
* **Function names.** The ~300 functions that had an address placeholder (`C_1184_02FA`, `__2E2D_0003`, `MUS_0008`...) now have real names (`OBJ_FindListPos`, `UseDoor`, `SPELL_Execute`, `GameLoop`...).
* **Global names.** The ~400 `D_xxxx` globals now have real names too (`ChunkCacheAge`, `PartyFrontMember`, `SpellPageNav`...). The address of every renamed function or global is still written in a comment in front of its definition and declaration, for example `/*C_1184_02FA*/OBJ_FindListPos(...)` or `/*06E6*/int MoveAppliesTerrain`. You can still match the source against a disassembly.
* **Locals and parameters.** Register and stack names (`si`, `di`, `bp06`, `bp_0a`...) are replaced by meaningful names in every function.
* **Constants.** Object types (`OBJ_SKIFF`, `OBJ_POWDER_KEG`...), tiles, spells (`SPELL_FIREBALL`...), commands, AI modes and monster flags have names instead of numbers. Struct members such as `_06` and `f_02` have names as well.
* **Comments.** Every function has a comment saying what it does. Every module has a description at the top, and the declarations in the headers say what each global holds. Old comments that turned out to be wrong were fixed.
* **File names.** The C files are renamed after their content (table below). `MAKEFILE`, `u6.lnk` and `mydiff.bat` are updated, keeping the original link order.
* **Findings.** [`FINDINGS.md`](FINDINGS.md) lists the probable bugs and quirks of the original game that turned up while reading the code.

Not changed: control flow, expressions, declaration order, data, the assembly code (only symbols shared with C were renamed), the OSILIB file names, and `padding.c`.

| new file | old file | content |
|---|---|---|
| `main.c` | `seg_0903.c` | Startup, memory and file loading |
| `gameloop.c` | `seg_0A33.c` | Main game loop, world clock and display tick |
| `console.c` | `seg_0C9C.c` | Console text output and user input |
| `map.c` | `seg_101C.c` | Map chunks and party relocation |
| `visible.c` | `seg_1100.c` | Line of sight and lighting |
| `object.c` | `seg_1184.c` | Object list management |
| `status.c` | `seg_155D.c` | Status panel: party, portrait, equipment, inventory |
| `talk.c` | `seg_16E1.c` | Conversation entry |
| `talkdr.c` | `seg_1703.c` | Conversation script interpreter (talkdr) |
| `spell.c` | `seg_1944.c` | Spells and spellbook |
| `npctrack.c` | `seg_1E0F.c` | NPC tracking: movement, schedules, path finding, A.I. dispatch |
| `combat.c` | `seg_2337.c` | Combat: attacks, damage, death and combat AI |
| `objcmd.c` | `seg_27a1.c` | Object commands: look, drop, get, move, use |
| `egg.c` | `seg_2E2D.c` | Creature generation (eggs) |
| `music.c` | `seg_2F1A.c` | Music driver interface and speaker sound effects |
| `gfx.c` | `seg_2FC1.c` | Graphic effects: driver init, missiles, explosions, sky bar, portraits, peer |
| `rest.c` | `seg_3200.c` | Rest command: camping, eating, night encounters, healing, hull repair |
| `disk.c` | `seg_32C3.c` | Floppy disk / drive management |
| `monsdata.c` | `seg_3522.c` | Monster class data tables |
| `msgs.c` | `seg_356A.c` | Far message strings |
| `gamestat.c` | `D_2C4A.c` | Saved game state variables |

### Checking that nothing changed
The Turbo C toolchain was not available for this rewrite, so equivalence was checked mechanically instead,
using `tools/verify.py` (Python 3, `gcc` for its preprocessor, `pip install pycparser==2.22`):

    git worktree add ../u6-main main
    python3 tools/verify.py ../u6-main/SRC SRC tools/rename_map.json

For every C translation unit, the script preprocesses the old and the new source and compares the token streams.
* Every token must be identical, except identifiers that follow the rename map and local variables renamed consistently inside one function.
* Number spellings may differ only if they have the same value and the same 16-bit C type.

It also compares the assembly files against the map. It then checks that no two public symbols collide when case is ignored
(TLINK is case-insensitive and `-p` uppercases names) and that no name is longer than 31 characters.
Codegen only depends on the token stream, so passing this check means the compiled output is the same.
Still, a real rebuild with `doit.bat` plus `mydiff.bat` is the final word.

---

## Native port for Windows and Linux (branch `portable`)

The same sources also build a native Windows/Linux version on SDL2, with a settings screen
over the game (F11), character creation, and a recording of every session that replays
exactly. The DOS build stays byte-identical. See [docs/port/PORT.md](docs/port/PORT.md).

---

## U.EXE decompiled (branch `uexe`)

`SRC_U/` is the decompilation of `U.EXE`, the other big program of the game: the main menu,
the introduction, the gypsy's character creation, the transfer of a character from Ultima IV
or V, the credits, and Origin's OSI library under them. It rebuilds an `U.EXE` identical to
the original, header and relocation table included:

    python3 tools/u/ubuild.py

See [SRC_U/DECOMP.md](SRC_U/DECOMP.md) for how it is put together and what it needs.
