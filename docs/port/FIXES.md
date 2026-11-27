# Fixes of the original game's faults

The native port can correct the faults of the original program that the code review rated
critical (docs/CODE_REVIEW.md, section 5.1). Every fix is **off by default**: off, the port plays
exactly as DOS does, and that is what the replay tests prove. A player turns them on all at once:

    u6 --fixes all                 # this session
    u6 --fixes list                # what each one does

or for good, in `u6port.cfg` in the home folder:

    fixes=all

A list of names turns on only those (`--fixes vortex-cube,keg-fuse`); `-name` turns one off
(`--fixes all,-lens-beam`); `none` turns them all off. The finding's number also works as a name
(`--fixes cr-004`).

## What turning them on changes

| Name | Finding | The original | With the fix |
|---|---|---|---|
| `vortex-cube` | CR-001 | Dropping any eight items whose frames are 0 to 7 onto the vortex cube ends the game like the eight moonstones: eight potions, or a mix such as two moonstones and six potions (a potion's colour and a moonstone's phase are the same frame, below). An item with a frame of 8 or more writes past an 8-entry array on the stack. | The cube counts only moonstones. |
| `inventory-scroll` | CR-002 | With 13 or more items carried, readying an item from an open bag and then scrolling down hangs the game for good. | Scrolling stops at the end of the bag's items. |
| `cast-target` | CR-003 | Casting at a creature or item on the map (the mouse, with the spellbook open) takes one away from a byte of memory chosen by the target's object number, not from the spell's count. | The spell's own line of the spellbook is counted down. |
| `keg-fuse` | CR-004 | When a lit powder keg is gone before its fuse runs out (blown up, vanished, doused, left on another level), whatever object takes its place is blown up 3 minutes later, quest items included. | The fuse only blows up a lit powder keg. A keg left lit on another level stays lit and harmless. |
| `healer-body` | CR-005 | A healer raises the first dead body found anywhere after the carrier in the object list: a temporary creature's corpse before the friend's. The gold is taken, the friend stays dead, and the creature's slot is brought back to life. | The healer raises a body the party member carries, and only a person's. A creature's corpse alone: the gold is still taken (the conversation script takes it first), but nothing is raised. |
| `lens-beam` | CR-006 | At the edge of the view a lens beam is put outside the view's tables. In practice one beam is missing there; with an NPC in a certain cell of the area, the object cache would be corrupted. | The beams are drawn where they belong. |
| `full-object-table` | CR-007 | When an area does not fit in the object table (2,815 objects) as it loads, the rest of it is dropped, and the next save writes the areas without it. A test lost 1,111 objects: 93 of a crowded area and all of Britain's (944) and the area beside it (74). | The objects that do not fit stay in the area's file and are written back with it. They are not in the world while it is that full. |
| `fountain-wish` | CR-008 | A wish at a fountain is compared with 10 entries of a 5-entry table; an empty wish matches one of the 5 read past it and can grant food. | Only the five wishes listed (Food, Mutton, Wine, Fruit, Meat). |
| `map-seam` | CR-010 | Where the map wraps around (x 1023 to 0), the object cache's order is not the object list's, and an object moved there is linked after the wrong one. A test with a ship at each edge of the ocean put the list out of order. | An object moved there is linked in the right place. |

**Potions and moonstones in the vortex cube.** The cube's test reads each item's frame, never its
type. A potion's frame is its colour and a moonstone's is its phase, frame for frame (sessions
`potions`, `potions2` and `potionsmix`; docs/GAME_MECHANICS.md, the endgame trigger):

| Frame | Potion | Moonstone (virtue, moongate) |
|---|---|---|
| 0 | orange, awaken | Honesty |
| 1 | red, cure | Compassion |
| 2 | yellow, heal | Valor |
| 3 | green, poison | Justice |
| 4 | blue, sleep | Sacrifice |
| 5 | purple, protection | Honor |
| 6 | black, invisibility | Spirituality |
| 7 | white, x-ray vision | Humility |

Not here:
- **CR-009**: the talk interpreter's read below its stack is harmless in DOS by accident, and the
  port makes it harmless by design, unconditionally (SRC/talkdr.c, gate 3 of 3). Nothing to turn on.
- **CR-008's crash on the host** (the 5 pointers past the table) is also handled unconditionally
  (SRC/u6port.h's FOUNTAIN_WISH). The fix above only changes what an empty wish does.

## Enhancements

Changes to what the original does by design, not faults. They are kept apart from the fixes, so
`--fixes all` does not turn them on. They are also off by default and are recorded and replayed
the same way.

    u6 --enhance moonstone-guard   # or enhance=moonstone-guard in u6port.cfg; --enhance list

| Name | The original | With it |
|---|---|---|
| `moonstone-guard` | Using a moonstone on grass or earth buries it at once ("buried."). From then on, its phase's moongates lead there, and its own moongate stands on it, so the player can lose track of where it is. Burying is not permanent: Get takes it back when both moons are down. | The game asks "Bury the moonstone here?" first (Y or N, as "Save game?" does). N leaves it in the pack: "Not buried." |
| `grouped-finds` | Looking at a body or an open container beside you searches it and lists each object: "a throwing axe, a throwing axe, a throwing axe, …" (throwing weapons come as 6 to 12 separate objects). | Identical items are listed once with their count: "throwing axes (7x), a wooden shield, a leather helm, a chest and 35 gold". The objects still fall onto the tile one by one, as before. |
| `look-tile` | Look describes only the top object of a tile. | After the description, a line lists everything on the tile, top first, repeats grouped: "Here: 35 gold coins, a chest, a leather helm, a wooden shield, throwing axes (7x) and a dead body." |
| `choose-get` | Get takes the top object of the tile. | If the tile holds more than one object, Get shows the top one and the arrow keys step through the others (each on a line of its own, since the message window is narrow). Enter or Space takes the one shown, Esc gives up. |

The last three are in objcmd.c (EnhTileObjs and EnhPrintGrouped; SearchObj, CmdLook, CmdGet).
A name without the game's plural endings gets English's ("axe" to "axes"). Two sessions test them.
Each was recorded in DOS without the enhancements, and the port matches it in all four builds.
Typed through the port's key feeder with the enhancements on (`rp.py port-record OUT NAME
--enhance grouped-finds,look-tile,choose-get`), the run is DOS's up to the first enhanced
message:
- `headless` (dungeon level 2, Mass Kill on a headless, a troll and two cyclopses; the troll's
  body searched, the tile looked at, then Get with two downs takes the leather helm, where DOS
  takes the gold);
- `chestbag` (the chest at (298, 367): Use spills 18 items, Look lists them, Get with four
  downs takes the bag).

The moonstone guard is a few lines in objcmd.c's UseMoonstone, under the same `U6_FIX` guard
(`ENH_MOONSTONE_GUARD`). The `moonstone` session tests it:
- **Off:** DOS buries the stone, `MoonstonePos[3]` becomes (309,379), and the port is identical
  in all four builds.
- **On** (`rp.py port-record OUT moonstone --enhance moonstone-guard`): N leaves
  `MoonstonePos[3]` at its moongate (295,38); Y sets it.
  - The recording carries the name (format 5), and its `--replay` is identical.

## How a fix is built

- **In the shared sources:** each fix is a few lines in SRC under `#ifndef __TURBOC__`, guarded by
  `U6_FIX(FIX_...)` (SRC/u6port.h). Neither DOS build sees them, so:
  - GAME.EXE stays byte-identical to the original (`tools/gate.py check`: 3 of 3);
  - the replay DOS build keeps its size (the same sha1), so every recording stays valid.
- **The registry:** port/sys/fixes.c, which holds the names, the parsing and the list.
- **The flags are fixed for the session:** they are set before the game starts and never change
  while it runs.
- **Recordings and replays:**
  - A session played with fixes is recorded with their names (replay.c's format 5).
  - `--replay` of such a recording turns the same fixes on, so it replays exactly.
  - The DOS build reads up to format 4, so DOS cannot replay a session with fixes; it was never
    the original's run.

The sites, as SRC has them (search for `fix (CR-`):

| Fix | Where |
|---|---|
| CR-001 | objcmd.c, UseVortexCube |
| CR-002 | status.c, STAT_DrawInventory's skip loop |
| CR-003 | spell.c, SPELL_CastFromBook and TakeReagents |
| CR-004 | gameloop.c, the fuse in PassTime |
| CR-005 | talkdr.c, OP_RESURRECT |
| CR-006 | object.c, ShowObjects (both lenses) |
| CR-007 | object.c: OBJ_LoadArea and OBJ_LoadDungeon keep the rest (FixKeepRest); OBJ_WriteBlockFile writes it back (FixPutRest); OBJ_DeleteTmpBlocks drops it when a game is restored |
| CR-008 | objcmd.c, UseFountain |
| CR-010 | object.c, OBJ_FindListPos |

## How they are tested

Each critical finding has a test session whose recording was made in DOS (or, for CR-003's mouse
click, in the port and replayed in DOS). tests/replay/sessions.toml lists them.

- **Off (`rp.py port-check`):**
  - every session, the bug sessions included, is identical to DOS in all four builds (Linux and
    Windows, gcc and clang): every checkpoint and the saved games.
- **On (`rp.py port-check --rec R --session S --fixes all`):** the port replays the DOS
  recording with every fix on.
  - The 15 sessions that reach none of the faults are still identical to DOS, every checkpoint
    and save: the fixes change nothing else.
  - Each bug session goes its own way exactly where its fault is:

| Session | Off (as DOS) | On |
|---|---|---|
| potions | the Codex vanishes for eight potions | the Codex stays |
| invhang | the game hangs | the session ends (it is not a test session: run with port-record) |
| cr003 | `FormationObjs[3]` (DS:EBD8) goes from 00 to FF | Heal's count goes from 99 to 98; DS:EBD8 untouched |
| keg | the stalagmite in the keg's old slot is blown up | the stalagmite stays |
| cr005 | slot E5h (a creature) is raised; NPC 6 stays dead | NPC 6 is raised |
| cr006, cr006b | the beam's node is put on the view's free list (`CellObjNext`) | the free list stays whole |
| cr007 | the saved area files: CC 2771 of 2864, CD 0 of 944, BD 0 of 74 | all 2864, 944 and 74 kept |
| fountain | the empty wish is compared past the table and can grant food | it is not |
| cr010 | the object list is out of order at checkpoint 17 | it stays in order |

The scratch checks that read those values from the dumps are described in the session comments.
