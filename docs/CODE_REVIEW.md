# Code review of the decompiled GAME.EXE

Reviewed on 2026-11-26. Nothing in the code was changed. The only file the review added is this one.

## 1. What was reviewed

`SRC/*.c` and `SRC/*.h` hold the C source of Ultima VI's `GAME.EXE` (Origin, 1990), recovered by
decompilation. Built with Turbo C 2.0 (16-bit `int`, medium model, far pointers, packed structs),
the files rebuild `GAME.EXE` byte for byte, and `tools/gate.py` proves it. **What the code does is
therefore what the 1990 game does.** A defect described here is a defect of the original program,
not of the decompilation. No finding gave reason to suspect a decompilation error.

The same C is also compiled natively for 64-bit Windows and Linux by `port/`. The port must behave
exactly like DOS: every recorded session must replay identically in both. Several facts about the
port decide whether a construct is a hazard:

- It builds with `-std=gnu89 -fsigned-char -fwrapv -fno-strict-aliasing` (`port/CMakeLists.txt`).
  Signed overflow wraps, plain `char` is signed and aliasing is permitted, so those classes of
  undefined behaviour are not hazards here. `-Wreturn-type` and `-Wint-conversion` are only warnings.
- The uninitialised data (BSS) lives in one block, `u6_bss`, in DOS order. The monster tables and
  the saved-game state are also laid out as in DOS (`gen_bss.py`, `gen_block.py`). A small read or
  write past a BSS array therefore hits the same neighbour as in DOS.
- **Initialised** tables (`DirIncrX`, `SpreadDirTable`, `FountainWishes`, `CircleNames`, `TinyFont`,
  `DistanceTable`, ...) are ordinary separate C objects. Pointers in them are 8 bytes instead of 2.
  `port/game/dgroup_port.c` only maps them to their DOS offsets for the replay dumps. An
  out-of-bounds read of such a table reads different bytes, or wild pointers, on the host.
- `STACK_JUNK_SET`/`STACK_JUNK_FILL` (`SRC/u6port.h`, Exhume's `portable.h`) give a fixed value to
  the few locals the original reads before setting them. Today they cover `containerPos.raw[2]`
  (object.c) and `spr_x`/`spr_y` (gfx.c).

## 2. How

Every line of these files was read, in seven parallel passes: combat.c, monsdata.c, objcmd.c,
object.c, map.c, visible.c, egg.c, spell.c, rest.c, npctrack.c, gameloop.c, talk.c, talkdr.c,
main.c, disk.c, music.c, msgs.c, gamestat.c, console.c, status.c and gfx.c. Headers, `BSS.ASM`, the
port's layout files and the callers or callees needed for each finding were read as required. The
most severe findings and many of the others were then checked again by hand against the source.
The reviewers inspected some findings that depend on scripts or map data against the shipped files
(CONVERSE.A/B through `tools/dialogue_export.py`, the objblk files of a recorded save), using
read-only scripts. Nothing was compiled, run or played.

The review looked for two groups of defects:

- **Group A, bugs of the 1990 game.** Wrong logic or operator, off-by-one, wrong variable,
  precedence, missing `break`, inverted test, table indexed past its end, signed/unsigned confusion,
  overflow that changes an outcome, dead code that was meant to run, objects lost or duplicated,
  soft-locks.
- **Group B, hazards for the native port.** Behaviour that differs on a 64-bit host or is undefined
  in C: uninitialised reads not covered by `STACK_JUNK*`, out-of-bounds accesses whose neighbour
  differs on the host, reliance on 16-bit `int` or 2-byte pointers, pointer/integer casts, missing
  return values, printf-format injection, NULL reads (DOS reads DS:0).

`FINDINGS.md` and `docs/GAME_MECHANICS.md` (written during the readability pass) already list a
number of these. The **Known** column says where each finding was already recorded:

- **source**: a comment at the spot (`bug`, `bug?`, `note`);
- **FINDINGS**: `FINDINGS.md` or `docs/GAME_MECHANICS.md`;
- **new**: first recorded here.

### Severity and confidence

- **critical**: crash, corruption of the object lists or the saved game, soft-lock, or loss of a
  quest item.
- **high**: a wrong outcome a player would notice (combat, spells, items, NPCs, quest).
- **medium**: wrong, but minor or rare.
- **low**: cosmetic, dead code, latent, or harmless.

Severity rates the consequence. A critical finding can still need a rare trigger, and the trigger
is stated with each one.

- **confirmed**: the code path and its effect were read and verified.
- **likely**: the code path is verified, but the effect also depends on runtime state or data
  that was not observed.
- **possible**: plausible from the code. The finding says what would confirm it.

## 3. Limits

- Nothing was run. Each "likely" or "possible" finding states the in-game or debugger check that
  would settle it. The DOS stack layout of functions with local arrays (CR-001, CR-039) was not
  derived from a disassembly.
- The map, schedule, red-gate and terrain data were only sampled. Findings that depend on data
  values are marked as such.
- The OSI library and the drivers (assembly, and its C rewrite under `port/sys`) were not reviewed.
  Neither were `SRC_U` (U.EXE) and `SRC_END` (END.EXE), apart from their interface with GAME.EXE.
- The internals of `Missiles()` from the combat side, `PartyTeleport`/`GateTravel`, and
  `COMBAT_canSee` were read less closely than the rest.

## 4. Summary table

| Id | Sev. | Grp | Known | Where | Title |
|---|---|---|---|---|---|
| CR-001 | critical | A+B | new | objcmd.c:3166 | Vortex cube indexes an 8-entry stack array by any item's frame |
| CR-002 | critical | A | new | status.c:374 | Inventory scroll loop never ends when InvenSize belongs to another owner |
| CR-003 | critical | A+B | source, FINDINGS | spell.c:2766, 675 | Casting at a map target decrements `SpellPageCounts[objectNumber]` |
| CR-004 | critical | A | new | objcmd.c:1998, 2050; gameloop.c:828; spell.c:938 | Powder keg fuse keeps a stale object number and blows up whatever reuses it |
| CR-005 | critical | A | new | talkdr.c:949 | Healer's resurrection revives the first dead body anywhere after the NPC in the chain |
| CR-006 | critical | A | new | object.c:2004, 2018 | Lens beam guards swapped: writes `Obj_11x11[y][-1]`, can corrupt the object cache |
| CR-007 | critical | A | new | object.c:1557 | Object table full while loading a block: the rest of the block is lost for good |
| CR-008 | critical | B | source (A side) | objcmd.c:2639 | Fountain reads 5 pointers past `FountainWishes`: wild pointers on the host |
| CR-009 | critical | B | FINDINGS (DOS side) | talkdr.c:583 | Every string LET from an array (all shops) reads and writes `lstack[-1]` |
| CR-010 | critical | A | new | object.c:225-262 | Object-cache ordering breaks when the 40x40 window crosses the map seam |
| CR-011 | high | A | source, FINDINGS | npctrack.c:2072 | Thief mode overwritten before tested: thieves never steal |
| CR-012 | high | A | new | objcmd.c:2480 | A rune frees a shrine of only one gargoyle egg |
| CR-013 | high | A | new | combat.c:1630, 1299 | AI picks a bow or crossbow without ammunition and stands still |
| CR-014 | high | A | new | status.c:206 | "Immovable" weight sentinel 255 tested after multiplying by the quantity |
| CR-015 | high | A | new | spell.c:2714, 2761 | Map-click cast after paging the spellbook skips the reagent check |
| CR-016 | high | A | FINDINGS | spell.c:2095 | Pickpocket `KARMA -= 5` wraps an unsigned byte to 251-255 |
| CR-017 | high | A | FINDINGS (cause), new (effect) | talkdr.c:296 | Dismissing a companion in combat does not adjust `Active` |
| CR-018 | high | A | new | talkdr.c:1184, 1281 | "look" during a Seance runs the script from a stale offset |
| CR-019 | high | A | new | object.c:1805; map.c:189 | Object cache not rebuilt after a new region is loaded |
| CR-020 | high | B | new | object.c:750 | `AddObj` has no `return`, yet about 25 callers use its value |
| CR-021 | high | A+B | new | egg.c:84, 165 | Monster possessions read from `MonsterInvLists[MonsterInvIndex[-1]]` |
| CR-022 | high | A | new | objcmd.c:853, 881, 896, 1242 | Dropping part of a stack takes from the first matching stack (duplication) |
| CR-023 | medium | A | new | spell.c:1711 | Mass Awaken restores the shape of non-sleepers (a mounted rider loses the horse) |
| CR-024 | medium | A | new | rest.c:389 | Without a bard, only the first member eats at mealtime |
| CR-025 | medium | A | FINDINGS | rest.c:492 | Members after the first unfed one are not healed |
| CR-026 | medium | A | FINDINGS | rest.c:332 | Last party member cannot be chosen as guard by number |
| CR-027 | medium | A | new | spell.c:1787 | A monster's Fear spares GOOD targets, so it never scares the party |
| CR-028 | medium | A | new | spell.c:2532 | Armageddon skips slot 0xFF and leaves `Active`/`SoloMember` stale |
| CR-029 | medium | A+B | new | spell.c:1224 and others | `EGG_generate` failure (-1) unchecked; Animate deletes the item anyway |
| CR-030 | medium | A+B | new | combat.c:801 | Wake-up code in `COMBAT_showHit` runs for items (clears a CHARMED bit, garbage shape) |
| CR-031 | medium | A+B | new | combat.c:608 | AI triple crossbow at range 6-7 indexes `SpreadDirTable[11][11]` out of bounds |
| CR-032 | medium | A+B | FINDINGS | combat.c:1757 | `EnemyFrontFirst == 0` tested where "none" is -1 |
| CR-033 | medium | A | new | combat.c:951 | A broken mirror can be broken again, 10 karma each time |
| CR-034 | medium | A | new | combat.c:848 | Karma lost for any NEUTRAL death, whoever the killer |
| CR-035 | medium | A+B | new | objcmd.c:637 | Looking at a book prints the text of every object on the tile with a quality |
| CR-036 | medium | A+B | new | objcmd.c:919, 3241 | Get/Use on an empty tile reads object -1 (can pick up a remote item) |
| CR-037 | medium | A | new | objcmd.c:1613 | Using a single torch in a bag marks it EQUIP while it stays in the bag |
| CR-038 | medium | A | new | objcmd.c:970 | Getting a lit torch skips the free-hand check when another member is displayed |
| CR-039 | medium | A+B | new | objcmd.c:2949 | Crystal ball `CON_gets(buf, 4)` into `char buf[4]` |
| CR-040 | medium | A | new | npctrack.c:1997, 2231 | Path slots leak across a level change and stall every schedule |
| CR-041 | medium | A | new | npctrack.c:2289 | Party centre rounding uses `PartySize` instead of `num` |
| CR-042 | medium | A | new | npctrack.c:2281 | Party centre is garbage aboard a vehicle |
| CR-043 | medium | A | new | npctrack.c:2299, 2329 | Combat geometry counts actors on other levels and empty slots |
| CR-044 | medium | A | new | npctrack.c:1923 | Surrendering to an arrest stands down only the guard who asked |
| CR-045 | medium | A+B | source, FINDINGS | npctrack.c:1865 | "Blocked next to goal" indexes `DirIncrX` with the raw path byte |
| CR-046 | medium | A | new | egg.c:426 | No egg hatches near x or y = 0 (AreaX wraps to 1008) |
| CR-047 | medium | A | FINDINGS | egg.c:382 | `npcMod` taken from the first embryo applies to all later ones |
| CR-048 | medium | A | source, FINDINGS | gfx.c:456 | `break` in `Missiles()` makes the remaining projectiles skip a step |
| CR-049 | medium | A+B | new | status.c:196 | Gold above 32767 goes negative in weight and in the number drawn |
| CR-050 | medium | A+B | new | spell.c:767, 2215 | Enchant from a staff without a readied spellbook prints `CircleNames[-1]` |
| CR-051 | medium | B | FINDINGS | main.c:214 | `FileErrorHandler` decides retry/fail from an uninitialised `key` |
| CR-052 | medium | B | FINDINGS (script) | talkdr.c:597 | Peer's `data(@075d, $9)` uses a pointer's low byte as index; host value differs |
| CR-053 | medium | B | new | console.c:2084 | `$x` escape for an unset VarStr dereferences NULL |
| CR-054 | medium | B | new | talkdr.c:811 | `CON_printf(VarStr[i])` uses text as a format string |
| CR-055 | medium | A | source, FINDINGS | main.c:420 | End-game date arithmetic tests `Date_D < 0`, wraps months at 13 |
| CR-056 | medium | A | new | npctrack.c:624, 695 | Followers struggle and pay movement twice per leader step |
| CR-057 | medium | A | source, FINDINGS | objcmd.c:2579 | Fishing pole tests `actorY + i - 1` (only diagonals) |
| CR-058 | medium | A | source, FINDINGS | objcmd.c:1919 | Sleep potion tests UNDEAD instead of sleep immunity |
| CR-059 | medium | A+B | new | objcmd.c:1883 | Potion on an object reads `NPCStatus[obj]`, which aliases `ObjStatus` |
| CR-060 | medium | A | new | objcmd.c:832, 1230; status.c:287 | `Equipment[-1]` overwrites `FreeMonsterCount` |
| CR-061 | medium | A+B | new | gfx.c:592, 321 | Monster-cast winds index `SpreadDirTable`/`MissileAngles` out of bounds |
| CR-062 | low | A | source, FINDINGS | objcmd.c:296 | `(!TalkFlags[n] & 1)` is `TalkFlags[n] == 0` |
| CR-063 | low | A | source, FINDINGS | objcmd.c:2458 | Balloon placement skips the corner instead of the centre |
| CR-064 | low | A | source | objcmd.c:1136, 1151 | Cannon `(a != b) & 3`: odd but no effect |
| CR-065 | low | A | source, FINDINGS | disk.c:117 | Ctrl-C at the disk prompt uses a drive number as a letter |
| CR-066 | low | A+B | new | combat.c:377, 389 | A dead dragon is deleted twice (`Link[-1]` read) |
| CR-067 | low | A | new | combat.c:615 | Triple crossbow charges its 3 bolts to the first stack only |
| CR-068 | low | A | FINDINGS | combat.c:2070 | A GOOD berserker only attacks EVIL actors |
| CR-069 | low | B | FINDINGS | combat.c:362, 994 | `MonsterFlags[-1]` for actors without a class |
| CR-070 | low | B | new | combat.c:1796 | Front-line projection overflows 16 bits in DOS, not on the host |
| CR-071 | low | A | new | combat.c:1895 | `COMBAT_AI_Rear` counts freed slots and other levels |
| CR-072 | low | A | new | combat.c:2310 | Tangle vine can grow twice in one turn |
| CR-073 | low | A | new | combat.c:134 | `COMBAT_begin` tests the bound after reading `Party[partyIdx]` |
| CR-074 | low | A | new | combat.c:872 | Party removal loop runs to `i <= PartySize` |
| CR-075 | low | A | new | combat.c:168 | `COMBAT_breakOff` stores a party index in `Leader[]` |
| CR-076 | low | A | new | monsdata.c:198, 233 | Peasant and woman spawn with "Cloth" (an item) as armour |
| CR-077 | low | A | new | spell.c:1001 | Chain Bolt can hop onto the caster and the party |
| CR-078 | low | A | new | spell.c:2316, 1625 | Tremor and the slime spell hit only EVIL, not CHAOTIC |
| CR-079 | low | A | new | spell.c:2522 | Wind Change sets the wind even when "nowhere" is answered |
| CR-080 | low | A | new | spell.c:1689 | Mass spells use a stale `SpellResult` for "Success" |
| CR-081 | low | A | new | spell.c:2260, 2277 | Wizard Eye uses the 6th-circle effect and allows 41 moves |
| CR-082 | low | A | new | spell.c:1157, 1666 | Vanish/Reappear loses quantity, quality and contents |
| CR-083 | low | A | new | spell.c:540, 560 | Spell object quality >= 0x80 writes past `SpellList[8]` |
| CR-084 | low | A | new | spell.c:2209 | Enchant's "staff full" test is `== 10` |
| CR-085 | low | A | FINDINGS | spell.c:2199 | Enchant tests `obj == -1 && obj >= 0x100` |
| CR-086 | low | A | FINDINGS | spell.c:729 | `SpellPageNav[0] = prevCircle - SpellPageNav[1]` |
| CR-087 | low | A | FINDINGS | rest.c:456 | Night-encounter test `!(i & 5)` |
| CR-088 | low | A | new | spell.c:1198 | Paralyze has no resistance roll |
| CR-089 | low | A | new | spell.c:2706 | Spell target path can run with a stale `Selection` |
| CR-090 | low | A | new | spell.c:2068 | Aborted pickpocket from a staff leaves the victim's inventory displayed |
| CR-091 | low | B | FINDINGS | spell.c:826, 1076, 1814, 2317, 2340 | Reads at index -1 (class or object) |
| CR-092 | low | A | new | spell.c:867; rest.c:183 | Dead branch and asymmetric spawn offsets |
| CR-093 | low | A+B | new | objcmd.c:3005 | Telescope with no direction leaves the view moved |
| CR-094 | low | A+B | new | objcmd.c (many); egg.c:87 | `AddObj`/`AddInvObj` returning -1 never checked |
| CR-095 | low | A | new | objcmd.c:2333, 2373 | Lever/switch loop can stop early after a delete |
| CR-096 | low | A | new | objcmd.c:968 | Get on a stackable reads the deleted object |
| CR-097 | low | A | new | objcmd.c:2209 | Drawbridge loops forever if no bridge end lies below |
| CR-098 | low | A | new | objcmd.c (various) | Cosmetic and consistency issues in object commands |
| CR-099 | low | B | new | objcmd.c:495, 2483, 3287, 2871 | Further reads of object -1 |
| CR-100 | low | A | new | console.c:711 | Portrait column uses `PointerY - 28`, equipment column `- 24` |
| CR-101 | low | A | new | console.c:2059 | `*` text escape leaves `PendingSelect` set |
| CR-102 | low | A | new | console.c:1893 | "more" prompt can flush the line before word wrap |
| CR-103 | low | A | new | console.c:1922 and others | Dead code in the console |
| CR-104 | low | A+B | new | console.c:522 | Key glyph `TinyFont[qual + 9]` past the table for quality > 16 |
| CR-105 | low | B | FINDINGS | console.c:155 | `CON_mouseOffAt` returns an uninitialised value |
| CR-106 | low | A | source | console.c:1321, 1561 | `SPELL_RefreshList` called with arguments it ignores |
| CR-107 | low | A+B | new | status.c:85 | `ComModeNames` indexed without a range check, always for `Party[StatusMember]` |
| CR-108 | low | A | new | gfx.c:1048 | Peer map's west/east edge handling is incomplete |
| CR-109 | low | A | new | gfx.c:476 | A projectile that left the view is drawn at its last position |
| CR-110 | low | A | source, FINDINGS | gfx.c:658, 723 | Hailstorm edge and flash-position quirks |
| CR-111 | low | A | new | gfx.c:509, 568, 958 | Small Missiles/Explosion/LoadConversation edge cases |
| CR-112 | low | A | new | npctrack.c:618 | Wrong formation facing when Beh Lem leads |
| CR-113 | low | A | new | npctrack.c:1060 | A slide after a blocked diagonal is charged as a diagonal |
| CR-114 | low | A | source, FINDINGS | npctrack.c:789 | Unboard calls `ClrLocal(mapObj)` before testing `mapObj >= 0` |
| CR-115 | low | A | new | npctrack.c:1524 | Path search ignores the schedule's level |
| CR-116 | low | A | new | npctrack.c:1514 | Path search does not wrap area-relative coordinates |
| CR-117 | low | A | new | npctrack.c:2186 | Path resistance lets any NPC through doors |
| CR-118 | low | A+B | new | npctrack.c:886 | Red gate quality not validated |
| CR-119 | low | A | new | npctrack.c:780 | Stowed skiff stays with actor 0 after leaving a ship on land |
| CR-120 | low | A | source, FINDINGS | npctrack.c:1986 | `TalkToParty` passes an object number as a party index |
| CR-121 | low | A | new | npctrack.c:2087, 1303 | Unreachable `AI_SEEKOBJ` path would overrun |
| CR-122 | low | A | new | npctrack.c:1237, 2245 | Dead results and wrong operand order |
| CR-123 | low | B | new | npctrack.c:2024 | Schedule indexed for temporary monsters |
| CR-124 | low | A+B | new | talkdr.c:400 | `OP_WEIGHT` product is 16-bit in DOS, 32-bit on the host |
| CR-125 | low | A | FINDINGS (partly) | talkdr.c:1045 | Answered-KEY skip does not skip operands |
| CR-126 | low | A | new | talkdr.c:1004 | `OP_SHOW_INVENTORY` never clears `ShowNPCInven` |
| CR-127 | low | A | new | talkdr.c:213 | `TALK_GetCharName` returns `Names[8]` when not found |
| CR-128 | low | A | new | talkdr.c:232 | `IsNpcOnScreen` ignores level and wrap |
| CR-129 | low | A | new | talkdr.c:977 | Innkeeper heals before the night passes |
| CR-130 | low | A | new | main.c:402, 445, 533 | Configuration string handling |
| CR-131 | low | A | new | music.c:211, 281 | Read before bound test; negative random bound |
| CR-132 | low | A | new | gameloop.c:356 | Unreachable daemon..mongbat test |
| CR-133 | low | A | FINDINGS | talkdr.c:1166 | Winged gargoyles accepted for talk but never mapped |
| CR-134 | low | B | FINDINGS | talk.c:78 | `GetType(-1)` with no selection |
| CR-135 | low | A | new | egg.c:329 | Serpent body built around a deleted head |
| CR-136 | low | A | new | object.c:1160, 906 | `InsertObj`/`DeleteObj` do not handle `prev < 0` |
| CR-137 | low | A+B | new | object.c:589, 657 | Serpent movement: table read past its end, `backDir` uninitialised |
| CR-138 | low | A | new | object.c:939, 1951; visible.c:223 | Object purge and display do not wrap at the seam |
| CR-139 | low | B | new | object.c:1261 | `GiveObj` returns an uninitialised `obj` for quantity 0 |
| CR-140 | low | B | new | object.c:405, 505, 868 | `FindInv`/`FixMapObjPtr` read index -1 |
| CR-141 | low | A+B | FINDINGS | visible.c:156, 178 | Peer mode reads `DistanceTable[8][8]` up to [23][23] |
| CR-142 | low | A | source, FINDINGS | visible.c:280 | Light-on-wall test checks the same square twice |
| CR-143 | low | A | source, FINDINGS | object.c:2084 | Free list gets 0x100 when slot 0xFF is used |
| CR-144 | low | A | source | object.c:1653 | Computed `relX`/`relY` unused |
| CR-145 | low | A | FINDINGS | object.c:1200 | `QuanType` given the frame bits |
| CR-146 | low | B | new | combat.c:1968 | `bestSide = 0x8000` into an `int16` |

Totals: 10 critical, 12 high, 39 medium, 85 low (146 in all). By group: 105 in A, 17 in B and
24 in both. 106 are new; the other 40 were already recorded, at least in part, in a source
comment, `FINDINGS.md` or `docs/GAME_MECHANICS.md`.

## 5. Findings

Line numbers are those of the current tree. Each code quote is copied from the file.

### 5.1 Critical

#### CR-001: Vortex cube indexes an 8-entry stack array by any item's frame (A+B, new)
objcmd.c:3160-3167, `UseVortexCube`
```c
int16 hasStone[8];
...
for(stone = FindInv(objNum); stone >= 0; stone = NextInv())
    hasStone[GetFrame(stone)] = 1;
```
- **Defect.** The loop trusts that the cube holds only moonstones (frames 0-7). Only `CmdMove`
  from the inventory enforces that (objcmd.c:1286). `CanPutInContainer` accepts any item into the
  cube, so dropping an item onto a cube on the map, or pushing one onto it, puts anything inside.
- **Trigger and effect.**
  - (a) Any item with frame 8 or more (`GetFrame` is 0..63) writes 2 bytes up to 110 bytes past
    the array, on the stack. That can hit the saved BP or the return address: a likely crash in
    DOS and undefined behaviour on the host.
  - (b) Eight items with frames 0-7 that are not moonstones, for example potions of the eight
    colours, satisfy the test. With the lenses and the Codex in place, the game ends without the
    moonstones.
- **Confidence.** Confirmed from the code. Which stack slot is hit needs the Turbo C frame layout
  (a disassembly of `C_27A1_5FAC`).
- **Fix sketch.** Count only `GetType(stone) == OBJ_MOONSTONE` with frame < 8.

#### CR-002: Inventory scroll loop never ends (A, new)
status.c:374-378, with `STAT_Ready`/`STAT_Unready` (status.c:649, 736) and console.c:797
```c
while(InvenScroll > slot) {
    if(GetCoordUse(item) == INVEN || GetCoordUse(item) == CONTAINED)
        slot ++;
    item = NextInv();
}
```
- **Defect.** `STAT_Ready`/`STAT_Unready` end with `STAT_RefreshInventory(Party[StatusMember])`.
  That sets `InvenSize` to the member's own item count, while `InvenOwner` can still be an open
  bag. The scroll-down test uses `InvenSize`.
- **Trigger.** The member carries 13 or more items. Open a bag holding 3 or fewer, ready an item
  from it (or unready any item), then scroll down.
- **Effect.** `InvenScroll` becomes 4 and the skip loop runs past the bag's last item, so
  `item = -1`. From then on `GetCoordUse(-1)` reads `NPCStatus[0xFF]`, the same in DOS and the
  port, and `NextInv()` keeps returning -1. Unless temporary actor 0xFF happens to have bit 0x08
  or 0x10 set, `slot` never grows and the game hangs.
- **Confidence.** Likely; the code path is verified. Confirm by playing the steps above.
- **Fix sketch.** Refresh `InvenOwner` instead of `Party[StatusMember]`, and leave the loop when
  `item < 0`.

#### CR-003: Casting at a map target decrements `SpellPageCounts[objectNumber]` (A+B, source and FINDINGS)
spell.c:2761-2766, and `TakeReagents` at spell.c:675
```c
} else if((SpellPageCounts[SPELL_FindPageLine(spellNum)] & 0x7f) == 0) {/*castable count*/
...
    /*(when targetPreset, Selection.obj is the target, not a page line)*/
    TakeReagents(caster, spellNum, Selection.obj);
```
`TakeReagents` then runs `SpellPageCounts[index] --;`.
- **Defect.** `SpellPageCounts` has 8 bytes. With the spellbook open, clicking the map sets
  `Selection.obj` to the object under the pointer, and that object number is used as the index.
- **Trigger.** Cast, then click a creature or an item instead of a spell line.
- **Effect.** One byte of BSS is decremented at `0xEBBE + objNum`. Depending on the object number
  this lands in path tables, `MUS_Driver`, `GrDriver`, the `GrInfo` far pointer, and for objects
  from 0x122 on, beyond the end of BSS. In the port that last case is outside `u6_bss`, which is
  undefined behaviour.
- **Confidence.** Confirmed that the path exists. Which neighbour is hit for common targets can be
  shown with an ASan build of the port (Cast, click an item).
- **Fix sketch.** When `targetPreset` is set, pass `SPELL_FindPageLine(spellNum)` and skip the
  decrement if it is -1.

#### CR-004: Powder keg fuse keeps a stale object number (A, new)
objcmd.c:1998-1999 and 2050-2067 (`Detonate`), gameloop.c:828-832, spell.c:938-950
```c
KegFuse = 3;  KegObject = objNum;            /* objcmd.c UsePowderKeg; same in spell.c Ignite */
...
} else if(KegFuse) { KegFuse = 0; Detonate(KegObject); }   /* gameloop.c */
```
- **Defect.** Nothing checks, when the fuse runs out, that `KegObject` is still the lit keg.
  `Detonate` never clears `KegFuse`. Several paths free the keg or change it while the fuse runs:
  - the lit keg is blown up early by another explosion (`AfterExplosion`: a fumarole, the
    Explosion or Mass Kill spell, a second keg);
  - Vanish (`DeleteObj`);
  - a level change frees the slot;
  - Douse sets the frame back to 0 but leaves `KegFuse` running;
  - Ignite on a second keg overwrites `KegObject`, so the first keg stays lit forever.
- **Effect.** About 3 minutes later `Detonate` runs on the slot. Freed slots go to the head of the
  free list, so the slot has probably been reused, and that object is deleted with an explosion at
  its position. The victim can be a quest item, including one in a pack: `Detonate` walks up to
  the holder. If the slot is still free, a phantom explosion follows and `DeleteObj` reads
  `Link[-1]`.
- **Confidence.** Likely. Confirm in game: light a keg, Vanish it, and watch which object
  disappears 3 minutes later.
- **Fix sketch.** Clear `KegFuse` in `Detonate`, in Douse and in Vanish of `KegObject`. Before
  detonating, check `GetType(KegObject) == OBJ_POWDER_KEG && GetFrame(KegObject) == 1`.

#### CR-005: Healer's resurrection revives the wrong body (A, new)
talkdr.c:949-954, `OP_RESURRECT`
```c
arg0 = mk_npcnum(parse_factor());
for(arg1 = Link[arg0]; arg1 >= 0; arg1 = Link[arg1])
    if(GetType(arg1) == OBJ_DEAD_BODY)
        break;
SPELL_ReviveNPC(arg1);
```
- **Defect.** The walk follows the global chain past the NPC's own inventory. It also lacks the
  filter the Resurrect spell has (spell.c:1397-1407): bodies of quality 0 or of 0xE0 and above are
  not people. `OBJ_MakeDeadBody` gives a temporary monster's body its slot number (0xE0 or more)
  as quality.
- **Trigger.** A member carries the body of a killed temporary creature (guard, fighter,
  villager), possibly together with a dead companion's body, and pays a healer (for example
  NPC 26) to resurrect.
- **Effect.** The first body in the chain is revived. Either the companion stays dead and the
  gold is taken anyway, or a temporary slot that may be free or already reused is un-killed and
  moved, which corrupts the object and free lists. Quality 0 revives object 0, the vehicle slot.
- **Confidence.** Likely. Confirm by carrying a creature's corpse to a healer.
- **Fix sketch.** Walk only `arg0`'s subtree (`FindInv`) and skip bodies of quality 0 or of 0xE0
  and above.

#### CR-006: Lens beam guards swapped (A, new)
object.c:2004-2007, and the same pattern at 2018-2021
```c
if(objX)
    ShowObject(TIL_1BA, objX + 1, objY, 1);
if(objX < 10)
    ShowObject(TIL_1BB, objX - 1, objY, 1);
```
- **Defect.** `objX != 0` guards `objX + 1`, and `objX < 10` guards `objX - 1`: the guards are
  the wrong way round.
- **Trigger.** The test `objX + MapX == 0x39e` holds for the placed lens wherever the party is.
  `objX == 0` (party 5 tiles east of a lens) writes `Obj_11x11[objY][-1]`. `objX == 10` writes
  `[objY][11]`.
- **Effect.** At best a stray beam on the wrong row. With `objY == 0` the write lands on the high
  byte of `MapObjPtr[39][39]` (0xE566, just before `Obj_11x11`). That cache entry then holds a
  bogus object number, which `OBJ_FindListPos` can follow: `Link[garbage]` reads and writes, and a
  broken chain until the next rebuild. The port has the same layout, so the same corruption.
- **Confidence.** The out-of-bounds write is confirmed from code and layout. Corruption needs the
  party at (0x39E,0x358) or (0x3A2,0x358) and a `Tile_11x11` value there that does not return
  early. Confirm by walking there and dumping `MapObjPtr[39][39]`.
- **Fix sketch.** Swap the guards (`objX < 10` before `+1`, `objX > 0` before `-1`).

#### CR-007: Object table full while loading a block (A, new)
object.c:1557-1562
```c
if(FreeObjCount == 0) {
    ScratchBuf->objNum[i] = -1;
    if(i)
        Link[ScratchBuf->objNum[i - 1]] = -1;
    break;
```
- **Defect.** The rest of the block is silently dropped, and the next `OBJ_UnloadArea` saves the
  block without it.
- **Effect.** Objects are lost permanently, quest items included.
- **Trigger.** Rare. The largest shipped 2x2 window holds 1931 objects against a limit of 2815,
  so about 880 extra objects (hoarding, Replicate, loot) are needed.
- **Confidence.** Confirmed from the code.
- **Fix sketch.** Refuse the region change, or keep the unread remainder in the .tmp file.

#### CR-008: Fountain reads 5 pointers past `FountainWishes` (B; A side flagged in source)
objcmd.c:2615-2640
```c
char *FountainWishes[] = { "Food", "Mutton", "Wine", "Fruit", "Meat" };
...
for(i = 0; i < 10/*bug?*/; i ++) {
    if(stricmp(wish, FountainWishes[i]) == 0) {
```
- **Defect.** Every wish that matches none of the 5 entries reads entries 5-9.
- **DOS (A, low).** Those are the words at 0x1D49-0x1D52 (`UnboardDirX`/`UnboardDirY`), which act
  as near pointers to whatever DGROUP text sits there. A wish equal to that text would grant food.
- **Host (B, critical).** The table is 5 native 8-byte pointers followed by an unrelated object.
  Entries 5-9 are whatever 40 bytes follow it, passed to `stricmp` as pointers: a likely crash, or
  a different match.
- **Confidence.** The out-of-bounds read is confirmed. The host outcome depends on the link
  layout; confirm with an ASan build (wish for "xyz").
- **Fix sketch.** Port: give the host table 10 entries, holding pointers to the DOS bytes (or
  NULL with a guard). Fix flag: loop to 5.

#### CR-009: String LET from an array reads and writes `lstack[-1]` (B; DOS side in FINDINGS)
talkdr.c:347, 583-590
```c
NEARLONG lstack[10];/*local "stack"*/
...
case OP_DATA:/*read a 16bit value from array*/
    arrayAddr = lstack[sidx - 2];/*array address*/
    varIdx = lstack[sidx - 1];/*array index*/
    ...
    lstack[sidx - 2] = READ_S16;
```
- **Defect.** A string LET from an array reaches `OP_DATA` with one operand, so `sidx - 2 == -1`.
  The reviewers counted 86 such sites in the shipped scripts, including every shop.
- **DOS.** This works by accident: the slot below the array holds a stable value.
- **Host.** `lstack` is `int64_t[10]`. The code reads 8 bytes below the array (`arrayAddr`), reads
  script bytes from `TalkBuf + (int16)garbage`, and writes 8 bytes below the array. What is
  overwritten (padding, a spilled register, another local such as `sidx`) depends on the compiler
  and the optimisation level. Nothing covers it with `STACK_JUNK*`.
- **Confidence.** Confirmed undefined behaviour. The concrete damage needs ASan or a look at the
  generated code.
- **Port fix.** Under `!__TURBOC__`, reserve one slot below `lstack[0]` (an 11-element array
  addressed from element 1), filled with the value DOS has there.

#### CR-010: Object-cache ordering across the map seam (A, new)
object.c:225-242, 252-262 (`OBJ_FindListPos`)
```c
for(i = j - 1; i >= 0 && ((int16 *)MapObjPtr)[i] == -1; i --);
...
for(i = 0x63f; i >= 0 && ((int16 *)MapObjPtr)[i] == -1; i --);
```
- **Defect.** When the 40x40 window crosses x or y = 1023/0 (`AreaX` or `AreaY` around 1008),
  the cache order no longer matches the chain order, and inserts land after the wrong object.
- **Effect.** An out-of-order chain makes `OBJ_FindPrevLink` return -1. Then `MoveObj` refuses the
  move ("Object mismatch"), `DeleteObj` passes `Link[-1]`, and `InsertObj` writes `Link[-1]` and
  links the object twice (CR-136). Cycles and save corruption can follow.
- **Confidence.** Possible. The shipped surface has no objects within 20 tiles of the edges, so it
  needs a ship or balloon within about 16-23 tiles of a surface edge, or the dungeon seam. Confirm
  whether the party can get there, then check the chain order after dropping an item.
- **Fix sketch.** Compare positions with wrap-aware relative coordinates.

### 5.2 High

#### CR-011: Thieves never steal (A, source and FINDINGS)
npctrack.c:2072-2076
```c
NPCMode[objNum] = AI_LOITER;/*bug?*/
if(NPCMode[objNum] == AI_THIEF)
    StealGold(objNum, other);
else
    TalkToParty(objNum, other);
```
- **Effect.** `StealGold` is dead code: AI_THIEF NPCs start a conversation instead.
- **Confidence.** Confirmed.
- **Fix sketch.** Keep the mode in a local before overwriting it, and test the local.

#### CR-012: A rune frees a shrine of only one gargoyle egg (A, new)
objcmd.c:2480-2490, `DestroyGargoyleEggs`, with object.c:441 (`NextArea`)
```c
for(egg = SearchArea(x - 20, y - 20, x + 20, y + 20); egg >= 0; egg = NextArea()) {
    ...
        while(content >= 0) { DeleteObj(content); content = FindInv(egg); }
        DeleteObj(egg);
```
- **Defect.** `NextArea`'s `for` increment has already moved `AreaCursor` to `Link[egg]`, the
  egg's first content, before it returns the egg. Deleting that content relinks it into the free
  list, so the next `NextArea` walks free slots and stops.
- **Effect.** Only the first gargoyle egg in chain order is removed; the other eggs within 20
  tiles keep hatching gargoyles at the "freed" shrine.
- **Confidence.** Likely. Confirm at a shrine with two or more gargoyle eggs in range.
- **Fix sketch.** Collect the eggs first and delete them afterwards, or restart `SearchArea` after
  each deletion.

#### CR-013: AI picks a ranged weapon without ammunition (A, new)
combat.c:1630-1634 (`__PickWeapon`) and 1299-1302 (`__TryHit`)
```c
dmg = GetDamagePoints(i);
if(dmg > max_dmg && __GetWeaponRange(i) >= range) {
...
if((objTyp == OBJ_BOW || objTyp == OBJ_MAGIC_BOW) && FindInvType(att_objNum, OBJ_ARROW, -1) < 0)
    return 0;
```
- **Defect.** `__PickWeapon` never checks ammunition. A crossbow (12), bow (10) or magic bow (20)
  beats the melee weapons even against an adjacent target, and `__TryHit` then silently returns 0.
- **Effect.** A companion in AI combat mode, or a monster with a bow, that has run out of bolts or
  arrows stands still every turn.
- **Confidence.** Likely. Confirm with a companion holding a crossbow and no bolts.
- **Fix sketch.** In `__PickWeapon`, skip a bow or crossbow when its ammunition is missing.

#### CR-014: "Immovable" sentinel tested after multiplying (A, new)
status.c:202-207, `GetWeight`
```c
if(IsWeightPer10Type(objTyp))
    weight = (TypeWeight[objTyp] * qty) / 10;
else
    weight = TypeWeight[objTyp] * qty;
if(weight == 255)
    weight = 10000;
```
- **Defect.** The sentinel should be tested on `TypeWeight[objTyp]`, not on the total.
- **Effect.** Any stack whose total weight is exactly 255 counts as 10000. Examples: a stackable
  of weight 5 and quantity 51, or weight 3 and quantity 85; for gold, the quantities for which
  w*qty/10 gives 255. On the ground it says "Not possible" (objcmd.c:959). Carried, every later
  pickup says "too heavy".
- **Confidence.** Confirmed from the code. Which amounts trigger it depends on `TypeWeight`.
- **Fix sketch.** Test `TypeWeight[objTyp] == 255` before multiplying.

#### CR-015: Map-click cast after paging skips the reagent check (A, new)
spell.c:2714-2715 and 2761
```c
spellNum = SelectedSpell;
targetPreset = 1;
...
} else if((SpellPageCounts[SPELL_FindPageLine(spellNum)] & 0x7f) == 0) {
```
- **Defect.** The mouse page and circle buttons (console.c:1691-1755) change the page but not
  `SelectedSpell`. After paging away, `SPELL_FindPageLine` returns -1, and
  `SpellPageCounts[-1]` is `SpellbookPage[7]`, a spell number that is nearly always non-zero.
- **Effect.** Cast, page forward, click a creature: the spell is cast with no reagents, and
  `TakeReagents` removes none (it also triggers CR-003).
- **Confidence.** Likely. Confirm with a caster holding no reagents.
- **Fix sketch.** Treat a line of -1 as "No Reagents".

#### CR-016: Pickpocket wraps karma (A, FINDINGS)
spell.c:2095
```c
KARMA -= 5;
```
- **Defect.** `KARMA` is an `unsigned char`, and every other change to it clamps.
- **Effect.** At karma 0-4 it becomes 251-255. Shrines then praise the Avatar, and resurrection no
  longer costs experience (`KARMA < 98`).
- **Confidence.** Confirmed.
- **Fix sketch.** Use the clamping `SubKarma(5)`.

#### CR-017: Dismissing a companion in combat leaves `Active` stale (A; cause in FINDINGS, effect new)
talkdr.c:296-305, `LeaveParty`
```c
for(i = slot; i < PartySize; i ++) {
    Party[i] = Party[i + 1];
    strcpy(Names[i], Names[i + 1]);
}
PartySize --;
```
- **Defect.** In combat `Active` is the index of the member whose turn it is, and it is not
  adjusted here.
- **Trigger.** With `Active == 3` and `PartySize == 4`, member 3 talks to member 2 and dismisses
  them.
- **Effect.** `Active == PartySize` afterwards, so `IN_VEHICLE` becomes true. Commands answer "Not
  while aboard ship" and the prompt shows a stale name. If the stale `Party[3]` is 0, nothing
  repairs `Active`: a possible soft-lock. With `Active > slot`, control silently moves to the next
  member.
- **Confidence.** Likely. Confirm in combat mode.
- **Fix sketch.** `if(Active > slot) Active--;`

#### CR-018: "look" during a Seance runs the script from a stale offset (A, new)
talkdr.c:1184-1191 and 1281-1286
```c
if(!SeanceFlag) {
    ...
    TalkDescPC = Talk_PC;
...
if(stricmp(TalkInput, "look") == 0) {
    Talk_PC = TalkDescPC;
    parse_statement(TalkNpcs[0]);
```
- **Defect.** In a Seance `TalkDescPC` keeps the offset from the previous conversation, which
  belongs to another NPC's script.
- **Effect.** The current script runs from an arbitrary offset. It prints garbage and can execute
  SET, GIVEOBJ, KEY or LEAVE.
- **Confidence.** Confirmed from the code.
- **Fix sketch.** Set `TalkDescPC` in the Seance case too, or ignore "look" while `SeanceFlag` is
  set.

#### CR-019: Object cache not rebuilt after a region load (A, new)
object.c:1805-1815; map.c:189-190
```c
if(numLoaded)
    OBJ_RelinkHeldItems();
...
if(anX != BuiltAreaX || anY != BuiltAreaY || MapZ != BuiltAreaZ) {
    OBJ_RebuildMapObjPtr();
```
- **Defect.** Newly loaded objects are merged into the chain, but `MapObjPtr` is rebuilt only when
  the area origin changes. Walking west, the region window moves at relX = 4 while `AreaX` stays
  the same.
- **Effect.** For about 5 steps, the objects in the 16 columns just across a region boundary are
  inside the 40x40 window but not cached. `FindLoc` does not see them: no blocking, look, get or
  use on doors and chests there. This includes the blocking test of the step that crosses the
  boundary.
- **Confidence.** Likely, traced in the code. Confirm in the port by comparing `MapObjPtr` with a
  fresh rebuild after `LoadNewRegions`.
- **Fix sketch.** Rebuild (or set `BuiltAreaX = -1`) when `numLoaded != 0`.

#### CR-020: `AddObj` has no `return` (B, new)
object.c:750-755
```c
/*C_1184_134C*/AddObj(int16 status, struct coord location, int16 shapeType, int16 shapeNum, int16 amount) {
    if((status & 0x18) == LOCXYZ)
        AddMapObj(...);
    else
        AddInvObj(...);
}
```
- **Defect.** The function returns nothing, yet its callers use the value: objcmd.c:894, 1607,
  2127, 2327, 2330, 2371, 2433, 2452, 2692, 2925, 3060, 3087; spell.c:1238, 1666, 1764, 2160, 2269,
  2445; rest.c:352.
- **DOS.** It works because AX still holds the inner function's result.
- **Host.** The value is undefined (the build only warns). It works only while the compiler
  happens to emit a tail call or leave EAX intact.
- **Confidence.** Confirmed undefined behaviour.
- **Port fix.** `return AddMapObj(...)` / `return AddInvObj(...)`. That should compile to the same
  bytes under Turbo C; check with the gate.

#### CR-021: Possessions of a classless creature (A+B, new)
egg.c:84, called unconditionally from egg.c:165-225
```c
pList = &(MonsterInvLists[MonsterInvIndex[objClass]]);
```
- **Defect.** `EGG_generate` calls `EGG_GivePossessions` even when `objClass == -1`.
  `MonsterInvIndex[-1]` is the last byte pair of `MonsterFlags` (0x0800), so `pList` points 0x800
  bytes into the monster data segment, far past the 0x13E-byte table.
- **Effect.** Each non-zero byte there becomes a spell charge, weapon, armour or treasure. On the
  host the read leaves `u6_monsdata`, so the items differ from DOS.
- **Trigger.** Animate on an item type, or Clone of a type missing from `MonsterTypes` (for
  example `OBJ_MUSICIAN_SHORT_CAPE`, `OBJ_FARMER_2`).
- **Confidence.** Possible. The path is confirmed, but the DOS bytes at 3522:0B42 are unknown and
  may be zero. Dump them, or cast Animate and inspect the result.
- **Fix sketch.** `if(objClass >= 0) EGG_GivePossessions(...)`.

#### CR-022: Dropping part of a stack takes from the wrong stack (A, new)
objcmd.c:853, 881, 896 (`CmdDrop`) and 1242 (`CmdMove`)
```c
newObj = AddObj(0, xyz, objTyp, 0, qtyCode);
SetOkToGet(newObj);
TakeObj(actor, objTyp, qtyCode);
```
- **Defect.** `TakeObj` takes from the first matching stack in chain order, equipped items
  included, not from the stack the player selected.
- **Effect.** Use one torch of a stack, which creates a lit, equipped torch whose quantity is its
  burn time, at the head of the chain. Then drop 2 from the stack: 2 new torches appear on the
  ground, the burn time drops by 2, and the stack keeps its count (duplication). Stacks in
  different bags are confused in the same way.
- **Confidence.** Likely. Confirm with the torch steps.
- **Fix sketch.** Subtract from the selected object itself, as `CmdMove` does at 1300-1313.

### 5.3 Medium

**CR-023 (A, new): Mass Awaken restores the shape of non-sleepers.** spell.c:1711-1713
```c
case SPELL_MASS_AWAKEN:
    ClrAsleep(*victimPtr);
    ObjShapeType[*victimPtr] = OrigShapeType[*victimPtr];
```
- **Effect.** Dispel restores the shape only when the target is asleep (spell.c:1045); Mass Awaken
  restores it always. A mounted rider becomes a man on foot without `DismountHorse`, so the horse
  is lost and its rear half orphaned. Multi-tile monsters get the wrong frame.
- **Confidence.** Likely.
- **Fix sketch.** `if(IsAsleep(v))` first.

**CR-024 (A, new): Without a bard, only the first member eats.** rest.c:389-392
```c
MUS_SongIndex = 0;
noMeal = -1;
for(; MUS_SongIndex || (MUS_SongIndex == 0 && i == 0); i ++) {
```
- **Effect.** The loop lasts as long as the musician's melody, and `MUS_SongIndex` only advances
  while a bard (`OBJ_MUSICIAN_SHORT_CAPE`) plays. With no bard it runs once, so only member 0's
  food is used. `noMeal` becomes `PartySize`, so everyone heals. With a bard, members the song does
  not reach also count as fed.
- **Confidence.** Likely. Confirm by counting food before and after a rest.
- **Fix sketch.** After the loop, call `TryEat` for every member it did not reach.

**CR-025 (A, FINDINGS): Healing after rest stops at the first unfed member.** rest.c:492-496
- **Effect.** Members are healed for indexes `0..noMeal-1`, so everyone after the first hungry
  member goes unhealed, fed or not.
- **Fix sketch.** Track "fed" per member.

**CR-026 (A, FINDINGS): The last member cannot be chosen as guard by number.** rest.c:332
```c
if(!wasSelectMode && ch >= '1' && ch < PartySize + '0') {
```
- **Fix sketch.** `ch <= PartySize + '0'`.

**CR-027 (A, new): A monster's Fear never scares the party.** spell.c:1787-1798
```c
if(GetAlignment(objNum) == GOOD)
    return;
```
- **Effect.** Monsters cast Fear with `Party[0x10] = monster`. Fear spares GOOD targets, and party
  members are GOOD, so a monster's Fear only frightens other non-GOOD creatures near it.
- **Confidence.** Likely.
- **Fix sketch.** Spare targets of the caster's own alignment instead.

**CR-028 (A, new): Armageddon misses slot 0xFF and leaves `Active` stale.** spell.c:2532-2539
```c
for(objNum = 0; objNum < 0xff; objNum ++) {
    if(objNum != 1 && objNum != 5)/*Avatar, Lord British*/
        DeleteObj(objNum);
}
...
PartySize = 1;
```
- **Effect.** A creature in temporary slot 0xFF survives. If a companion casts the spell,
  `Active` stays pointing past the one-member party, and `IN_VEHICLE` can become true.
- **Confidence.** Slot 0xFF: confirmed. `Active`: possible (whether the combat code resets it was
  not traced).
- **Fix sketch.** Loop to `< 0x100`; set `Active = 0` and `SoloMember = -1`.

**CR-029 (A+B, new): `EGG_generate` returning -1 is not checked.** spell.c:1224-1226 (Animate),
1265 (Clone), 1873-1891 (Summon), 2118-2125 (Insect Swarm), rest.c:177-195, objcmd.c:2871-2877
(Dismount)
```c
hitOrNpc = EGG_generate(...); SetAlignment(hitOrNpc, ...); NPCMode[hitOrNpc] = AI_ASSAULT; DeleteObj(Selection.obj);
```
- **Trigger.** All 32 temporary slots are in use.
- **Effect.** Animate still deletes the item, and Dismount loses the horse. The calls write
  `NPCMode[-1]` and alignment bits at index -1, and `CanMoveTo(-1)` reads `ObjShapeType[-1]`. In
  rest.c a "monster" can wake the party without existing.
- **Confidence.** Possible. Confirm by filling all temporary slots, then casting Insect Swarm.
- **Fix sketch.** Return early on a negative result. In Animate, delete the item only after a
  successful generate.

**CR-030 (A+B, new): Wake-up code in `COMBAT_showHit` runs for items.** combat.c:798-805
```c
    if(objNum < 0x100)
        SetHurtThisTurn(objNum);
}
if(IsAsleep(objNum)) {
    ClrAsleep(objNum);
    ObjShapeType[objNum] = OrigShapeType[objNum];
```
- **Defect.** For an item N of 0x100 or more, `NPCStatus[N]` is `ObjStatus[N-0x100]`. For items
  0x100-0x1FF that byte belongs to actor N-0x100, where bit 0x04 means CHARMED.
  `OrigShapeType[N]` reads past its 0x100 entries.
- **Effect.** Hitting a breakable item can clear a charmed actor's CHARMED bit without restoring
  its alignment, and give the item a garbage shape, which is then saved.
- **Confidence.** Likely; the trigger is rare.
- **Fix sketch.** Guard the block with `objNum < 0x100`.

**CR-031 (A+B, new): Triple crossbow at range 6-7 reads past `SpreadDirTable`.** combat.c:608
```c
spreadDirs = SpreadDirTable[*pY_dest - GetY(att_objNum) + 5][*pX_dest - GetX(att_objNum) + 5];
TripleBoltX[1] = *pX_dest + DirIncrX[spreadDirs >> 4];
```
- **Trigger.** The table covers offsets up to 5, but the weapon's range is 7. The player's aim is
  limited to the view, while the AI (Assault, Flank, Berserk) can target 6-7 tiles away.
- **Effect.** The read lands in neighbouring tables, and the nibbles (up to 15) then index
  `DirIncrX` out of bounds. The side bolts hit wrong tiles. On the host the neighbours differ
  (initialised data), so a replay diverges.
- **Confidence.** Confirmed path.
- **Fix sketch.** Clamp dx and dy to -5..5.

**CR-032 (A+B, FINDINGS): Wrong "none" test for `EnemyFrontFirst`.** combat.c:1757, 1771
```c
if((EnemiesNum == 0 || EnemyFrontFirst == 0) && !IsPlrControl(objNum)) {
...
    axisX = GetX(EnemyFrontFirst) - originX;
```
- **Effect.** "None" is -1 (npctrack.c:2321), so `GetX(-1)` reads `ObjPos[-1]`: the bytes of the
  `BaseTile` far pointer. On the host that is an 8-byte pointer, so the movement diverges.
- **Trigger.** Reachable when an actor becomes AI_FRONT after the geometry was computed in the
  same round.
- **Fix sketch.** Test `EnemyFrontFirst < 0`.

**CR-033 (A, new): A broken mirror can be broken again.** combat.c:951-966
- **Defect.** Broken doors (frame 12 and above) and broken chests (frame 3) are excluded from
  further damage; a broken mirror (frame 2) is not.
- **Effect.** Every further "break" costs another 10 karma.
- **Confidence.** Likely.
- **Fix sketch.** Add `!(objTyp == OBJ_MIRROR && GetFrame(objNum) == 2)` to the exclusions.

**CR-034 (A, new; may be intended): Karma is lost for any NEUTRAL death.** combat.c:848-850
```c
if(GetAlignment(objNum) == NEUTRAL)/*killing an innocent*/
    SubKarma(5);
```
- **Effect.** `LoseHP` does not know the attacker, so a wolf killing a deer, or a monster killing
  a villager, costs the player 5 karma.
- **Confidence.** Confirmed.
- **Fix sketch.** Apply the penalty only when the attacker is player-controlled.

**CR-035 (A+B, new): Look at a book prints the text of every object on the tile with a quality.**
objcmd.c:637-644
```c
for(other = FindLoc(Selection.x, Selection.y, MapZ); other >= 0; other = NextLoc()) {
    if(GetQual(other) == 0)
        continue;
    ...
    PrintBookText(other);
```
- **Defect.** There is no book or sign test on the other objects.
- **Effect.** A key of quality N on the tile prints book N. A quality beyond BOOK.DAT's entries
  reads text bytes as an offset and prints garbage. The text then reaches
  `CON_printf(TalkBuf)` as a format string (objcmd.c:400), which is undefined on the host if it
  contains `%`.
- **Confidence.** Confirmed for the extra text; the garbage path is likely.
- **Fix sketch.** Require `IsBook || IsSign`. Port: print with `"%s"`.

**CR-036 (A+B, new): Get or Use on an empty tile reads object -1.** objcmd.c:919-922 (also
3241-3250)
```c
if(
    (Selection.obj <= 0xff && Selection.x != -1) ||
    (IsTileIg(TILE_FRAME(Selection.obj)))
) Selection.obj = FindNonIgnoredObjAt(Selection.obj);
```
- **Defect.** `-1 <= 0xff` is true, so `GetX(-1)`/`GetY(-1)` decode the bytes of the `BaseTile`
  pointer as a position.
- **Effect.** "Get" checks the distance on the adjacent `Selection.x/y`, so it can pick up an item
  lying at that garbage position elsewhere on the map. On the host the pointer bytes differ.
- **Confidence.** The out-of-bounds read is confirmed; the remote pickup is possible.
- **Fix sketch.** Return early `if(Selection.obj < 0)`.

**CR-037 (A, new): Using a single torch in a bag "equips" it in the bag.** objcmd.c:1613-1620
```c
if(GetCoordUse(objNum) == INVEN || GetCoordUse(objNum) == CONTAINED) {
    ...
    SetCoordUse(objNum, EQUIP);
```
- **Effect.** The torch becomes an EQUIP object owned by a bag. `STAT_ComputeEquipment` never sees
  it, and saving turns it back into CONTAINED.
- **Confidence.** Likely.
- **Fix sketch.** `InsertObj(objNum, Party[StatusMember], EQUIP)`.

**CR-038 (A, new): Getting a lit torch skips the free-hand check.** objcmd.c:970-982
```c
if(Active == StatusMember) {
    if(Equipment[SLOT_RHND] == 0) ...
}
InsertObj(Selection.obj, actor, EQUIP);
```
- **Effect.** While another member's inventory is displayed, the active member can get a third
  item in hand. One equipped item then becomes unselectable.
- **Confidence.** Likely.
- **Fix sketch.** Count the actor's own hand items with `FindInv`.

**CR-039 (A+B, new): Crystal ball overflows its buffer.** objcmd.c:2949, 2966, 2975
```c
char buf[4];
...
CON_gets(buf, 4);
```
- **Defect.** `CON_gets` stores up to `maxLen` characters plus a terminator (console.c:1784, 1802).
- **Effect.** Typing 4 characters writes `buf[4]`: in DOS a neighbouring local, on the host
  undefined behaviour.
- **Confidence.** Confirmed.
- **Fix sketch.** `char buf[5]`.

**CR-040 (A, new): Path slots leak across a level change.** npctrack.c:1997-2000, 2231-2233
```c
mode = NPCMode[PathObject[slot]];
if(!(mode >= AI_ONPATH  && mode <= AI_PATH_FAILED))
    PathObject[slot] = 0;
...
PTH_Index = __NewPathIndex();
if(PTH_Index == 8)
    return;
```
- **Defect.** An NPC left in a path mode on the old level gets no more turns, so its slot is
  never freed.
- **Effect.** With all 8 slots held, `PTH_SearchPaths` returns before searching and before its
  teleport loop. No NPC on the new level reaches its schedule until the slot holders' next
  schedule hour.
- **Confidence.** Likely. Confirm by changing level at a busy town hour.
- **Fix sketch.** Free slots whose NPC is on another level.

**CR-041 (A, new): Party centre rounding uses `PartySize`.** npctrack.c:2289-2290
```c
PartyGravityX = ((PartySize >> 1) + sum_x) / num;
```
- **Effect.** When members are in flank or berserk mode (`num < PartySize`), the centre moves up
  to 4 tiles south-east. The enemy centre (npctrack.c:2314) correctly uses `num >> 1`.
- **Confidence.** Confirmed.
- **Fix sketch.** `(num >> 1)`.

**CR-042 (A, new): Party centre aboard a vehicle.** npctrack.c:2281-2285
- **Defect.** Aboard, members sit in actor 0's inventory, so `GetX` decodes the owner number:
  x = 0.
- **Effect.** AI_FRONT enemies fail `TileDistance(..., PartyGravity) < 8` and wait instead of
  attacking a party at sea.
- **Confidence.** Likely.
- **Fix sketch.** When in a vehicle, use actor 0's position.

**CR-043 (A, new): Combat geometry counts other levels and empty slots.** npctrack.c:2299-2304,
2329-2335
- **Defect.** No `GetZ(i) == MapZ` test. The front loop has no `ObjShapeType` test, and
  `PartyFlankPos`/`PartyFlankNeg` are never reset.
- **Effect.** Phantom enemies and front lines; the formation AI degenerates into assault or
  waiting.
- **Confidence.** Confirmed in the code; the gameplay effect is possible.
- **Fix sketch.** Add level and shape filters; reset the flank variables each turn.

**CR-044 (A, new): Surrendering stands down only the asking guard.** npctrack.c:1923-1958
- **Defect.** "Yes, come quietly" sets only `NPCMode[guard] = AI_FINDPATH`. Other AI_ARREST NPCs
  stay in that mode (the schedule update skips it).
- **Effect.** A guard next to the cell, through the wall, can arrest again: the door is locked
  again, lockpicks are taken again, and time passes again.
- **Confidence.** Likely.
- **Fix sketch.** Set every AI_ARREST NPC to AI_FINDPATH.

**CR-045 (A+B, source and FINDINGS): "Blocked next to goal" uses the raw path byte.**
npctrack.c:1865
```c
if(GetX(objNum) + DirIncrX[info] == x && GetY(objNum) + DirIncrY[info] == y)
```
- **Defect.** `info` is dir + 4*count, so at least 4.
- **Effect.** The test checks the wrong square, so an NPC whose seat is occupied cycles
  blocked/failed/search instead of starting its action. Indexes of 8 and more read past
  `DirIncrX`. On the host that is initialised data in a different layout: a replay divergence.
- **Fix sketch.** `DirIncrX[(info & 3) << 1]`.

**CR-046 (A, new): No egg hatches near x or y = 0.** egg.c:426-428
```c
x = AreaX;
y = AreaY;
for(objNum = SearchArea(x, y, x + 39, y + 39); objNum >= 0; objNum = NextArea()) {
```
- **Defect.** `AreaX = (MapX - 0x10) & 0x3f8` is 1008 or 1016 when `MapX < 16`. Underground, the
  masked x (255 at most) never reaches that range.
- **Effect.** Underground, no egg in the whole area hatches. On the surface, eggs near 0 do not
  hatch. Shipped dungeon blocks contain dozens of eggs at x or y 1..23.
- **Confidence.** Confirmed from code and data.
- **Fix sketch.** Wrap-aware rectangle test.

**CR-047 (A, FINDINGS): `npcMod` is not reset per embryo.** egg.c:382-383. 67 shipped eggs have
more than one embryo.
- **Fix sketch.** Reset it at the top of the embryo loop.

**CR-048 (A, source and FINDINGS): `break` in `Missiles()`.** gfx.c:456-460
- **Effect.** When one projectile stops, the others skip that step. In Explosion near walls the
  last trails can fall short and miss NPCs on their last tile.
- **Fix sketch.** `continue` instead of `break`.

**CR-049 (A+B, new): Gold above 32767.** status.c:196, 203; console.c:494-526; object.c:1242
(`Amount[obj] += amount`, no cap)
- **Effect.** The weight turns negative, so the party can carry more. The digit drawn is
  `TinyFont[negative]`.
- **Port.** `TypeWeight * qty` is a 16-bit product in DOS and 32-bit on the host. They differ once
  the product exceeds 32767.
- **Confidence.** Confirmed; the trigger is rare.
- **Fix sketch.** Cap gold at 32767.

**CR-050 (A+B, new): Enchant without a readied spellbook.** spell.c:2215-2229, 767
```c
CON_printf("%s", CircleNames[pageCircle]);
```
- **Trigger.** Enchant cast from a staff (`UseStaff`) with no readied book: `pageCircle` stays -1.
- **Effect.** DOS prints the string at DS:0. On the host `CircleNames[-1]` is a wild pointer: a
  likely crash.
- **Confidence.** Likely.
- **Fix sketch.** Refuse when no spellbook is readied; guard `pageCircle < 0`.

**CR-051 (B, FINDINGS): `FileErrorHandler` reads an uninitialised `key`.** main.c:214-241
```c
int16 key;
...
if(key != 3 && key != 27)
    OSI_ErrorCode = 0;
```
- **Defect.** `key` is never set on the paths where the console is not ready yet or the disk is
  full.
- **Effect.** A missing file early in startup is retried or failed depending on stack junk. A
  retry loops forever, and the port's behaviour is nondeterministic.
- **Fix.** `STACK_JUNK_SET(key, ...)` for the port; fix flag: `key = 27`.

**CR-052 (B, script bug in FINDINGS): Peer's array index is a pointer's low byte.**
talkdr.c:597-599, 583-587
```c
lstack[sidx - 1] = PTR2INT16(VarStr[varIdx]);
```
- **Defect.** Peer (NPC 53) writes `$9` where `#9` was meant.
- **DOS.** The index is 0x31, from `TalkStrBuf[9]` at DS:EB31.
- **Host.** The index is the low byte of a host address, so his carry check reads a different
  word and a replay diverges.
- **Port fix.** Make `PTR2INT16` of a `VarStr` pointer give the DOS offset.

**CR-053 (B, new): `$x` escape for an unset `VarStr`.** console.c:2084-2092
```c
CON_PutsRaw(VarStr[varIdx]);
```
- **Defect.** `VarStr` entries start as NULL, and only G, N, P, T, Y, Z and $0-$9 are ever set.
- **Effect.** Any text with `$` and an unset letter prints junk from DS:0 in DOS and crashes the
  port. No string in SRC does this; data text could.
- **Confidence.** Possible.

**CR-054 (B, new): Text used as a format string.** talkdr.c:811
```c
CON_printf(VarStr[varIdx]);
```
- **Defect.** A variable typed by the player (`$Z`, GETSTR) that contains `%s` would read a
  missing argument.
- **Effect.** Undefined on the host. No shipped script prints a `$` variable this way.
- **Fix.** `CON_printf("%s", ...)`.

**CR-055 (A, source and FINDINGS): End-game date arithmetic.** main.c:420-428
```c
if(Date_D < 0/*should be "days"?bug?*/) {
    Date_D += 28;
    monthes --;
}
if(monthes < 0) {
    monthes += 13;
```
- **Defect.** `Date_D` is unsigned, so the test never holds. Months should wrap at 12.
  `CON_itoa(days, ..., 10)` prints a negative value as an unsigned number.
- **Effect.** END.EXE shows a wrong time spent in Britannia, and may index `numberWords[]` out of
  bounds (FINDINGS).
- **Fix sketch.** Test and fix `days`; use `+= 12`.

**CR-056 (A, new): Followers struggle and pay twice.** npctrack.c:624, 644, 695-696
- **Defect.** Both passes of the follow loop run the drag-under struggle and `SubMov(..., 5)`.
- **Effect.** A follower held by a corpser takes damage twice per step. Each follower pays 10
  points per step instead of 5, which slows the whole party.
- **Fix sketch.** Do both in pass 0 only.

**CR-057 (A, source and FINDINGS): Fishing pole tests only diagonals.** objcmd.c:2579
```c
if(IsTerrainWet(GetTileAtXYZ(actorX + i - 1, actorY + i - 1, MapZ))) {
```
- **Fix sketch.** `actorY + j - 1`.

**CR-058 (A, source and FINDINGS): Sleep potion tests UNDEAD.** objcmd.c:1919
- **Code.** `if(!IsMonsterUndead(GetMonsterClass(GetType(Selection.obj))))`
- **Effect.** Sleep-immune non-undead creatures fall asleep.
- **Fix sketch.** Test sleep immunity.

**CR-059 (A+B, new): A potion on an object aliases `NPCStatus`.** objcmd.c:1883 onward; also
CmdLook (590) and ShowPortraitAndInventory (220)
- **Defect.** `IsPlrControl(obj)` for an object number of 0x100 or more reads
  `ObjStatus[obj-0x100]`.
- **Effect.** If that byte matches, `SetPoisoned` and friends write status bits into `ObjStatus`.
  POISONED (0x08) is a CONTAINED location bit, so the object's location is corrupted.
- **Confidence.** Confirmed reading; the trigger is rare.
- **Fix sketch.** Reject `Selection.obj >= 0x100` first.

**CR-060 (A, new): `Equipment[-1]` overwrites `FreeMonsterCount`.** objcmd.c:832, 1230;
status.c:268-287
```c
Equipment[STAT_FindEquipSlot(obj)] = 0;
...
slot = STAT_GetEquipSlot(item); ... Equipment[slot] = item;
```
- **Defect.** Both functions return -1 when no slot matches, and `FreeMonsterCount` sits just
  before `Equipment` (BSS.ASM:164).
- **Effect.** A count of 0 stops `AddMonster` until reload; another value corrupts the count.
- **Confidence.** Possible. Reachable through stale `Equipment[]` after
  `ShowPortraitAndInventory`, or an NPC holding a readied charge.
- **Fix sketch.** Skip when the slot is < 0.

**CR-061 (A+B, new): Monster-cast winds index tables out of bounds.** gfx.c:592-603, 321
```c
sideDirs = SpreadDirTable[y - GetY(objNum) + 5][x - GetX(objNum) + 5];
```
- **Effect.** A monster can target 6 or more tiles away, past the 11x11 and 16x16 tables. Across
  the 0x3FF seam the indexes reach about ±1000. Wrong directions in DOS; reads of other
  globals, or a crash, on the host.
- **Confidence.** Possible. Confirm whether any monster can cast a wind spell.
- **Fix sketch.** Clamp the offsets.

### 5.4 Low

Each entry gives the place, the defect, its effect and a fix sketch. Confidence is "confirmed"
unless stated otherwise.

**CR-062 (A, source and FINDINGS).** objcmd.c:296.
- **Code.** `if(shrineTyp || (!TalkFlags[objNum] & 1))` means `TalkFlags[objNum] == 0`.
- **Effect.** An NPC with any talk flag set shows its real name before being met.
- **Fix sketch.** `!(TalkFlags[objNum] & 1)`.

**CR-063 (A, source and FINDINGS).** objcmd.c:2458.
- **Code.** `if(!i && !j) continue;` skips the corner (-1,-1), not the centre (1,1).
- **Effect.** The balloon may be tried on the actor's own tile.
- **Fix sketch.** `i == 1 && j == 1`.

**CR-064 (A, source).** objcmd.c:1136, 1151.
- **Code.** `((GetFrame(obj) != (AdvanceDir >> 1)) & 3)`
- **Effect.** None: `(x != y) & 3` equals `x != y`. Diagonal pushes map to the lower cardinal
  frame. No fix needed.

**CR-065 (A, source and FINDINGS).** disk.c:117-121.
- **Code.** `DiskDrive[diskNum] = CurrentDrive - ('A' - 1);` with `CurrentDrive` already a
  number (1..26).
- **Effect.** The stored drive is garbage, so the next disk prompt selects a wrong drive. Floppy
  setups only.
- **Fix sketch.** Store `CurrentDrive`.

**CR-066 (A+B, new).** combat.c:377 and 389.
- **Code.** Dragons have MONSTER_VANISHES and are deleted at 377, then
  `if(objType == OBJ_DRAGON) DeleteObj(objNum);` deletes them again.
- **Effect.** The second call finds no predecessor and reads `Link[-1]`; no harm in DOS.
- **Fix sketch.** `else if`.

**CR-067 (A, new).** combat.c:615-646.
- **Defect.** `numBolts = TestObj(...)` counts every stack, but only the first stack found is
  charged.
- **Effect.** 3 bolts fired from stacks of 1 and 20 cost 1.
- **Fix sketch.** `TakeObj(att_objNum, OBJ_BOLT, n)`.

**CR-068 (A, FINDINGS).** combat.c:2070-2072.
- **Defect.** `(objAln != GOOD || GetAlignment(i) == EVIL)`
- **Effect.** A party berserker ignores CHAOTIC monsters, which are most of them.
- **Fix sketch.** Use `Is_ATKPLR`/`Is_ATKMON`.

**CR-069 (B, FINDINGS).** combat.c:362, 994.
- **Defect.** `MonsterFlags[-1]` is read for `OBJ_FARMER_2` and `OBJ_MUSICIAN_SHORT_CAPE`. It
  holds 0x6000, so there is no effect, and the port keeps the layout.
- **Fix sketch.** Guard `objCls >= 0`.

**CR-070 (B, new, possible).** combat.c:1796-1797, 1914-1916.
- **Code.** `destX = (refDepth * axisX) / axisLen2 + originX;`
- **Effect.** DOS wraps the product at 16 bits before dividing; the host does not. It differs once
  the product exceeds 32767 (axes of about 26 tiles or more).
- **Port fix.** `(int16)(refDepth * axisX)`.

**CR-071 (A, new, possible).** combat.c:1895-1899.
- **Defect.** The front-depth loop has no `ObjShapeType` or level test, so freed slots and other
  levels count.
- **Fix sketch.** Add both tests.

**CR-072 (A, new, possible).** combat.c:2310-2331.
- **Defect.** No `return` after a successful directed grow, so the random grow can follow in the
  same turn.
- **Fix sketch.** `return` after the first successful grow.

**CR-073 (A, new).** combat.c:134-143.
- **Defect.** The loop condition reads `Party[partyIdx]` before testing
  `partyIdx < PartySize`.
- **Effect.** If every member is incapacitated, the stale `Party[PartySize]` gets AI_COMMAND.
- **Fix sketch.** Test the bound first.

**CR-074 (A, new).** combat.c:872.
- **Code.** `for(i = 1; i <= PartySize; i ++)`
- **Effect.** One entry past the party is read. Harmless, because it is a stale copy of a member.
- **Fix sketch.** `<`.

**CR-075 (A, new).** combat.c:168.
- **Code.** `Leader[Party[partyIdx]] = partyIdx;` stores an index where an actor is expected.
  Effectively dead.
- **Fix sketch.** `Party[partyIdx]`.

**CR-076 (A, new, possible data error).** monsdata.c:198, 233.
- **Defect.** Classes 0x05 and 0x10 list `OBJ_CLOTH` (0xB9, an item) as armour, where farmer and
  villager use `OBJ_CLOTH_ARMOR`. The mongbat (305-309) also gets weapons and armour.
- **Fix sketch.** `OBJ_CLOTH_ARMOR`.

**CR-077 (A, new).** spell.c:961-1013.
- **Defect.** `SPELL_FindChainTarget` has no alignment or party filter, so Chain Bolt can bounce
  onto the caster and the party. Only the first target is provoked, and the search window is ±4,
  not 5.
- **Fix sketch.** Skip player-controlled targets.

**CR-078 (A, new, possible).** spell.c:2316, 1625.
- **Code.** `GetAlignment(npc) == EVIL`
- **Effect.** CHAOTIC creatures are spared. Wing Strike and `REST_FoesNear` accept EVIL or
  CHAOTIC.
- **Fix sketch.** Accept CHAOTIC too.

**CR-079 (A, new).** spell.c:2518-2525.
- **Defect.** `WindDir = AdvanceDir;` runs after the "nowhere" branch too.
- **Fix sketch.** Move it into the else branch.

**CR-080 (A, new).** spell.c:1689-1734.
- **Defect.** `CastMass` never resets `SpellResult`, so "Success" depends on the previous spell.
- **Fix sketch.** `SpellResult = 0` first.

**CR-081 (A, new).** spell.c:2260, 2277.
- **Defect.** `SPELL_CastEffect(6)` for a 7th-circle spell, and `while(movesLeft >= 0 ...)` from
  40 gives 41 moves.
- **Fix sketch.** `MK_CIRCLE(SPELL_WIZARD_EYE)`; `> 0`.

**CR-082 (A, new; may be intended).** spell.c:1157-1160, 1666.
- **Defect.** Only the shape is kept, so Reappear makes quantity 1, quality 0 and an empty
  container. A key's lock id or a bag's contents are lost, and a second Vanish loses the first
  object.
- **Fix sketch.** Hide the object off-map instead of deleting it.

**CR-083 (A, new, possible).** spell.c:540-543, 560-563.
- **Code.** `SET_SPELL(spellQual >> 4, spellQual & 0xf);`
- **Effect.** A quality of 0x80-0xFE writes `SpellList[8..15]`. It needs bad data.
- **Fix sketch.** Skip qualities of 0x80 and above.

**CR-084 (A, new).** spell.c:2209-2210.
- **Code.** `if(i == 10)`
- **Effect.** A staff with more than 10 items passes the "full" test.
- **Fix sketch.** `>= 10`.

**CR-085 (A, FINDINGS).** spell.c:2199-2201.
- **Code.** `Selection.obj == -1 && Selection.obj >= 0x100` is never true, and the code then runs
  `GetType(-1)`.
- **Fix sketch.** `||`.

**CR-086 (A, FINDINGS).** spell.c:729-731.
- **Code.** `SpellPageNav[0] = prevCircle - SpellPageNav[1]`
- **Effect.** It gives prevCircle-2 when `[1] == 2`.
- **Fix sketch.** Per FINDINGS.md.

**CR-087 (A, FINDINGS).** rest.c:456-459.
- **Code.** `!(i & 5)`
- **Effect.** Only 2 of every 8 hours qualify for a night encounter.
- **Fix sketch.** `!(i & 3)`, or the intended mask.

**CR-088 (A, new; possibly intended).** spell.c:1198-1203.
- **Defect.** Paralyze has no resistance or immunity check, unlike Sleep, Charm and Kill.
- **Fix sketch.** Add `COMBAT_ResistsSpell`.

**CR-089 (A, new, possible).** spell.c:2706.
- **Code.** `if((ch != CMD_TARGET || Selection.obj == -1) && Selection.x == -1)`
- **Effect.** An unexpected key with a stale `Selection.x` takes the target path, which leads to
  CR-003.
- **Fix sketch.** Require `ch == CMD_TARGET`.

**CR-090 (A, new, possible).** spell.c:2068-2103.
- **Defect.** An aborted pickpocket leaves `InvenOwner` set to the victim. `CastFromBook` repairs
  this; casting from a staff does not.

**CR-091 (B, FINDINGS).** spell.c:826, 1076, 1814, 2317, 2340.
- **Defect.** `MonsterFlags[-1]` and `GetType(-1)` reads.
- **Effect.** Benign, and the same bytes in the port.
- **Fix sketch.** Guard the class or object.

**CR-092 (A, new).** spell.c:867-868; rest.c:183-186.
- **Defect.** The SPELL_ENERGY_WIND branch of `DirectDamage` is unreachable, and `damage` is
  uninitialised for other spells, which no caller passes. Night spawn offsets run from -6 to +4.

**CR-093 (A+B, new).** objcmd.c:3005-3016.
- **Defect.** The telescope sets `MapX`/`MapY` to its own position and returns on "nowhere"
  without restoring them.
- **Effect.** The view moves, and the distance checks use the wrong centre until the next step.
  From the inventory, the owner number is decoded as coordinates.
- **Fix sketch.** Restore the centre before returning.

**CR-094 (A+B, new).** objcmd.c:895, 1625, 2128, 2172, 2332, 2372, 2434, 2693, 2926, 3087;
egg.c:87-88.
- **Defect.** A -1 from a full object table is used unchecked. `ObjStatus[-1]` is
  `NPCStatus[0xFF]`, so `SetInvisible(-1)` paralyses the creature in slot 0xFF. `CmdDrop` also
  runs `TakeObj` after a failed add, so the item is lost.
- **Fix sketch.** Check for -1.

**CR-095 (A, new, possible).** objcmd.c:2333-2337, 2373-2377.
- **Defect.** Deleting an object during the lever and switch area walk strands the cursor, as in
  CR-012. UseLever's `break` on an NPC leaves the toggle half done.

**CR-096 (A, new).** objcmd.c:968-1004.
- **Defect.** `IsOkToGet` and `SetOkToGet` run on an object already passed to `DeleteObj`, and the
  child loop walks `Link[obj]`, which is now the free list. It works only because `DeleteObj`
  keeps `ObjStatus`.

**CR-097 (A, new).** objcmd.c:2209, 2228.
- **Defect.** `ToggleDrawbridge` searches down for a bridge end tile without a bound.
- **Effect.** A hang if none exists (data-dependent; not seen in the shipped map).

**CR-098 (A, new).** Object-command cosmetics:
- `UseBeehive` draws the inventory without the `CMD_STAT_EQUIP` guard (objcmd.c:1861).
- `UseTorch` checks the displayed member's hands for a torch on the ground (objcmd.c:1599).
- `AskHowMany` returns 1 for a readied stack but clears the whole slot (objcmd.c:743).
- `CmdMove` out of an NPC's bag inserts as CONTAINED (objcmd.c:1316).
- Moving a bag into itself prints "into ..." and charges movement.
- `SpringTrap` can delete a nested trap instead of the door's own trap (objcmd.c:1403).

**CR-099 (B, new).** More reads at index -1, the same bytes in the port's BSS:
- `SearchObj` (objcmd.c:495-503): `FindNonIgnoredObjAt` returns -1;
- `DestroyGargoyleEggs` with an empty egg (2483);
- `CmdUse` from an inventory object (3287);
- Dismount with full temporary slots (2871).

**CR-100 (A, new).** console.c:711 versus 716.
- **Code.** `SCR2TIL(PointerY - 28)` for the portrait column, `- 24` for the equipment column.
- **Effect.** The top 4 pixels of a row open the member above (on the first row, the Avatar).
- **Fix sketch.** Use `- 24`.

**CR-101 (A, new, possible).** console.c:2059-2064.
- **Defect.** The `*` wait discards a right-click's command but leaves `PendingSelect` set.
- **Effect.** The next `CON_getch` returns CMD_TARGET with a stale selection.
- **Fix sketch.** Clear `PendingSelect`.

**CR-102 (A, new, possible).** console.c:1893-1916.
- **Defect.** Status commands run during the "more" prompt flush the half-processed line.
- **Effect.** The last word is split instead of wrapped.

**CR-103 (A, new).** Dead code in the console:
- centring (`attrFlags & 2`) is never set and would not work if it were (console.c:1922);
- `unusedMidX` (1895);
- `if(i < 0)` on a byte (1553);
- `InGetString && ch == '\r'` (1241);
- the `%c` width.

**CR-104 (A+B, new, possible).** console.c:522.
- **Code.** `DrawTinyGlyph(GetQual(objNum) + 9, 7);`
- **Effect.** `TinyFont` has 26 glyphs, so a key quality above 16 reads past it (other bytes on
  the host) and quality 0 shows '9'. The shipped scripts use qualities 1-13.
- **Fix sketch.** Clamp.

**CR-105 (B, FINDINGS).** console.c:155-168.
- **Defect.** `inArea` is returned uninitialised without a mouse; the caller re-tests `MouseOn`.
- **Port fix.** `STACK_JUNK_SET` or `= 0`.

**CR-106 (A, source).** console.c:1321, 1561.
- **Defect.** `SPELL_RefreshList(Spellbook[0], Spellbook[1])` is passed arguments the function
  does not take; it reads the globals instead.
- **Effect.** Harmless; it would break only under C23 prototype rules.

**CR-107 (A+B, new).** status.c:85-91.
- **Code.** `ComModeNames[NPCComMode[objNum] - AI_COMMAND]` has no range check.
- **Effect.** It always shows `Party[StatusMember]`, even for a displayed NPC. A mode outside 2-8
  would read past the 7 entries (a wild pointer on the host).
- **Fix sketch.** Clamp.

**CR-108 (A, new, likely).** gfx.c:1048-1056.
- **Defect.** The peer map's west/east edge handling covers only `AreaX == 0x3f0` (with the wrong
  value) and `0x3e0`, and the `else if` chain skips the Y fix at corners.
- **Effect.** Off-map columns are drawn at the world edge.
- **Fix sketch.** Mirror the Y logic.

**CR-109 (A, new).** gfx.c:476-499.
- **Defect.** A projectile outside the view is still drawn at its last `spr_x`/`spr_y`. The port
  draws it at the `STACK_JUNK_FILL` value 0 until it moves into view.

**CR-110 (A, source and FINDINGS).** gfx.c:658-664, 723-727.
- **Defect.** Hailstones whose start position is negative (near world x 0) never fall. The flash
  position is converted in place, so a second NPC on the tile gets a wrong flash.

**CR-111 (A, new).** Small graphics edge cases:
- the boomerang's return test ignores y, but the caller ignores the result (gfx.c:509);
- 63 explosion hits overwrite the last entry (gfx.c:568);
- `LoadConversation` returns non-zero when `OSI_open` fails, so a stale `TalkBuf` runs
  (gfx.c:958).

**CR-112 (A, new).** npctrack.c:618-619.
- **Defect.** `facing_dir = (GetFrame(leader) >> 1) & 6` assumes 4 frames per facing, but
  `OBJ_GARGOYLE` has 3.
- **Effect.** The formation is wrong when Beh Lem leads.
- **Fix sketch.** `(frame / 3) << 1`.

**CR-113 (A, new).** npctrack.c:991-1008, 1060-1062.
- **Defect.** A slide after a blocked diagonal keeps the diagonal direction.
- **Effect.** 5 extra movement points and a diagonal facing.
- **Fix sketch.** Recompute the direction.

**CR-114 (A, source and FINDINGS).** npctrack.c:779-789.
- **Defect.** `ClrLocal(mapObj)` runs before the `>= 0` test. On a full object table this clears
  a bit of `NPCStatus[0xFF]`, and the vehicle vanishes.
- **Fix sketch.** Move it inside the test.

**CR-115 (A, new, possible).** npctrack.c:1524-1534.
- **Defect.** The schedule's z is never compared, so an NPC can walk to the right x,y on the wrong
  level and loop. Depends on the schedule data.

**CR-116 (A, new, possible).** npctrack.c:1514-1518, 1665, 663.
- **Defect.** The area-relative path coordinates are not masked with the level's wrap, unlike
  `RunNPCTurns`, so path finding fails near the underground seam.

**CR-117 (A, new, possible).** npctrack.c:2186-2200.
- **Defect.** Unlocked doors are passable in the resistance map for every NPC, while `CanMoveTo`
  lets only humans through.
- **Effect.** Animals on schedules loop blocked/failed.

**CR-118 (A+B, new, possible).** npctrack.c:886-889.
- **Code.** `RedGateX[gateIdx]` with the quality minus 1, unvalidated.
- **Effect.** Qualities 12-14 lead to (0,0,0), and above 25 read past the tables (other bytes on
  the host). Depends on the map data.

**CR-119 (A, new; may be intended).** npctrack.c:780-788.
- **Defect.** With `onLand` set, the stowed skiff stays in actor 0's inventory and follows the
  party to the next ship.

**CR-120 (A, source and FINDINGS).** npctrack.c:1986.
- **Code.** `TALK_talkTo(member, npc, 0)` passes an object number as a party index.
- **Effect.** Wrong `$P` name and stats.
- **Fix sketch.** Pass the index.

**CR-121 (A, new).** npctrack.c:2087-2089, 1303-1321.
- **Defect.** `AI_SEEKOBJ` is never set. If it were reached, `FurnitureTypes[Leader[]]` would
  read past 4 entries, and `PTH_BuildPath` could run off `PTH_map` with an uninitialised
  `dir_min`.

**CR-122 (A, new).** npctrack.c:1237, 2245.
- **Defect.** An unused `FindFurnitureHere` result.
  `while(PathObject[PTH_Index] && PTH_Index < 8)` reads `PathObject[8]` before the bound test.

**CR-123 (B, new, possible).** npctrack.c:2024.
- **Defect.** `Schedule[SchedIndex[o] + SchedPointer[o]]` is indexed for temporary monsters too.
  It stays within the 600-entry allocation unless the entries plus 255 exceed 600.

**CR-124 (A+B, new, latent).** talkdr.c:400-408.
- **Code.** `lstack[sidx - 2] = iparam0 * iparam2 * 10;`
- **Effect.** The product is 16-bit in DOS and 32-bit on the host. The shipped maximum is 32000,
  so no divergence today.
- **Port fix.** Cast to `int16`.

**CR-125 (A, FINDINGS partly; latent).** talkdr.c:1045-1049.
- **Defect.** The answered-KEY skip scans raw bytes without skipping operands. Only Arbeth
  (NPC 103) has a case, and it is unreachable.

**CR-126 (A, new, latent).** talkdr.c:1004-1007.
- **Defect.** `ShowNPCInven` is never cleared after `OP_SHOW_INVENTORY`. Only Iolo's debug menu
  uses the opcode.

**CR-127 (A, new).** talkdr.c:213-217.
- **Defect.** `TALK_GetCharName` returns the stale `Names[8]` when the object is not in the party.
- **Fix sketch.** Fall back to the NPC's name.

**CR-128 (A, new).** talkdr.c:232-243.
- **Defect.** `IsNpcOnScreen` ignores Z and wrap, so an NPC on another level counts as on screen.

**CR-129 (A, new, possible).** talkdr.c:977-985.
- **Defect.** The innkeeper heals before the night passes, so poison keeps working. At 5:xx a full
  24 hours pass.

**CR-130 (A, new).** main.c:402, 445-455, 533-535.
- **Defect.** Custom sound-card ports are lost in the configuration string, hex letters after the
  port are eaten, and a CONFIG.U6 shorter than 6 bytes is parsed from stack junk (B as well).

**CR-131 (A, new).** music.c:211, 281.
- **Defect.** `MUS_AmbientDist[pos]` is read before `pos < MUS_AmbientCount` is tested.
  `OSI_rand(200, 1500 - (dist << 8))` gets a negative upper bound at distance 6-7, which produces
  very high pitches.

**CR-132 (A, new).** gameloop.c:356.
- **Defect.** The daemon..mongbat range in the animal branch is unreachable, because line 225 takes
  it first.

**CR-133 (A, FINDINGS).** talkdr.c:1166.
- **Defect.** `OBJ_WINGED_GARGOYLE` is accepted for talk but never mapped, so it always answers
  "Funny, no response."

**CR-134 (B, FINDINGS).** talk.c:78.
- **Defect.** `GetType(-1)` with no selection. Harmless, and the same bytes in both builds.

**CR-135 (A, new, latent).** egg.c:329-336.
- **Defect.** A serpent body is built around a head that was just deleted, leaving permanent
  orphan segments. No shipped egg triggers it.
- **Fix sketch.** `continue` after `DeleteObj`.

**CR-136 (A, new, latent).** object.c:1160-1164, 906-919.
- **Defect.** `InsertObj` has no `prev < 0` check, so it writes `Link[-1]` (`SearchZ`) and links
  twice. `DeleteObj` passes `Link[-1]` on.
- **Trigger.** Only with an already broken chain (CR-010, CR-004, CR-066).
- **Fix sketch.** Return on `prev < 0`.

**CR-137 (A+B, new, possible).** object.c:589-598, 657-659.
- **Defect.** `DirConnClearMask[(GetFrame(tail) & 0xf) ^ 5]` can index 8-15 of a 7-byte table, and
  `backDir` is uninitialised if the head has no connection flag. Both need a broken serpent body.
- **Port fix.** `STACK_JUNK_SET(backDir, ...)`.

**CR-138 (A, new).** object.c:939-942, 1951; visible.c:223-227; object.c:306.
- **Defect.** The purge of far objects, the drawing and sight counting, and `NextLoc` do not wrap
  at the seam. At 1023, `SetCoordX(..., 1024)` spills into Y.
- **Effect.** Edge-only and minor.

**CR-139 (B, new).** object.c:1261-1268.
- **Defect.** `GiveObj` returns an uninitialised `obj` for a non-stackable item with quantity 0.
  The only such caller ignores the result.
- **Port fix.** `obj = -1`.

**CR-140 (B, new).** object.c:405-406, 505-506, 868.
- **Defect.** `GetCoordUse(-1)`/`GetAssoc(-1)` reads at the end of the chain. The result is
  unchanged, and the port's BSS has the same bytes.

**CR-141 (A+B, FINDINGS).** visible.c:156, 178-179.
- **Defect.** Peer mode reads `DistanceTable[8][8]` up to [23][23]. On the host that table is a
  separate static, so peer-map light values can differ from DOS.
- **Port fix.** Lay out `DistanceTable` and its DOS neighbours as in DOS.

**CR-142 (A, source and FINDINGS).** visible.c:280.
- **Code.** `(*(pFlags + AREA_W) & 0x80) && (*(pFlags + AREA_W) & 0x80)`. The intended second
  square is unknown.

**CR-143 (A, source and FINDINGS).** object.c:2084.
- **Defect.** When slot 0xFF is used, its predecessor gets the free-list link 0x100. Never
  followed, because the count reaches 0 first.

**CR-144 (A, source).** object.c:1653.
- **Defect.** `relX`/`relY` are computed and unused. Harmless.

**CR-145 (A, FINDINGS).** object.c:1200.
- **Code.** `QuanType(ObjShapeType[objNum])` includes the frame bits.
- **Effect.** A lit torch counts as 1.
- **Fix sketch.** `GetType`.

**CR-146 (B, new).** combat.c:1968.
- **Code.** `bestSide = 0x8000` into an `int16` is implementation-defined.
- **Effect.** It is -32768 on gcc, clang and MSVC, so there is no divergence today.

## 6. Recommendations

### 6.1 Port hazards to cover first

These can make the port crash or differ from DOS on ordinary play, so they come first:

1. **CR-009 (`lstack[-1]` in `OP_DATA`).** Every shop purchase runs it. Reserve a slot below the
   array on the host and give it the DOS value. Add a replay test with a purchase and a test built
   with ASan.
2. **CR-020 (`AddObj` has no return).** Make it return the inner result, which should be
   byte-identical under Turbo C (check with the gate). Turn `-Wreturn-type` back into an error for
   SRC, and see what else it finds.
3. **CR-008 (`FountainWishes[5..9]`).** Give the host table the DOS words 5-9 (as pointers to the
   bytes DOS points at), or NULL with a guard. Test with a failed wish.
4. **CR-003 / CR-015 (`SpellPageCounts[obj]`, `[-1]`).** On the host, an object of 0x122 or more
   writes past `u6_bss`. Pad `u6_bss`, or clamp the index under `!__TURBOC__` while still
   reproducing the DOS write, and add a test: Cast, then click an item.
5. **CR-050, CR-053, CR-054, CR-035 (format strings, NULL and wild string pointers).** Use `"%s"`
   and NULL guards on the host. DOS printed junk where the host crashes.
6. **CR-052 (pointer low byte as an index), CR-031, CR-061, CR-045, CR-118, CR-141, CR-104
   (out-of-bounds reads of initialised tables).** Lay out the affected initialised tables
   (`SpreadDirTable`, `MissileAngles`, `DirIncrX`/`DirIncrY`, `RedGate*`, `DistanceTable`,
   `TinyFont`) contiguously in DOS order, as the BSS already is. `PTR2INT16` of `VarStr` pointers
   should give the DOS offset.
7. **Uninitialised locals not covered by `STACK_JUNK*`.** These are `key` in `FileErrorHandler`
   (CR-051), `inArea` (CR-105), `backDir` (CR-137), `obj` in `GiveObj` (CR-139), `cfgString` past
   a short read (CR-130), `partyIdx` in `ShowPortraitAndInventory`, and the crystal ball's
   `buf[4]` overflow (CR-039). The first two are reachable in normal error paths.
8. **CR-070, CR-124 (16-bit products).** Add `(int16)` casts on the host where DOS wrapped a
   product before dividing.

### 6.2 Original bugs worth an optional fix flag (Phase 7)

These are the strongest candidates. Each has a clear fix and an effect players notice:

- crashes and object loss: CR-001, CR-002, CR-004, CR-005, CR-006, CR-007, CR-022;
- quest flow: CR-012 (gargoyle eggs), CR-018 (Seance look), CR-017 (dismissal in combat);
- items and economy: CR-014 (weight 255), CR-015 (free casts), CR-016 (karma wrap), CR-033 (mirror);
- AI: CR-011 (thieves), CR-013 (archers without ammunition), CR-040 (path slots), CR-041 (party
  centre), CR-044 (arrest), CR-045 (blocked next to goal), CR-056 (followers pay twice);
- spells: CR-023 (Mass Awaken), CR-027 (monster Fear), CR-028 (Armageddon), CR-029 (Animate);
- rest: CR-024 to CR-026 (meals, healing, guard);
- the long-known small ones that the community expects fixed: CR-057 (fishing), CR-058 (sleep
  potion), CR-062 (portrait name), CR-055 (end date).

Some findings may be design rather than bugs: CR-034 (karma for any neutral death), CR-068
(berserker), CR-078 (Tremor), CR-082 (Vanish), CR-088 (Paralyze). Flag them separately, as
gameplay changes rather than fixes.

### 6.3 Areas that deserve a second pass

- **Object chain integrity** (object.c `OBJ_FindListPos`, `InsertObj`, `DeleteObj`, the
  `NextArea`/`NextLoc` cursors while deleting). CR-004, CR-010, CR-012, CR-019, CR-095, CR-096 and
  CR-136 all come from the same fragility. A debug-build checker that walks the chain after every
  command would find more.
- **The map seam and the edges** (CR-010, CR-046, CR-108, CR-116, CR-138). Check first whether
  the surface edges are reachable.
- **Temporary-slot and object-table exhaustion** (CR-007, CR-029, CR-094). Run a stress session
  that fills both tables.
- **The OSI library and the drivers.** They were not reviewed here.
- **In-game confirmation** of the "likely" critical and high findings: CR-002, CR-004, CR-005,
  CR-012, CR-013, CR-017, CR-019, CR-022.
