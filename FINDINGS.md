# Findings from the readability pass

Every module was read in full while it was renamed. This file collects what came up along the way:
probable bugs in the original game, data-layout quirks and facts about how the game works.
**Nothing listed here was changed in the code**, because the source must keep compiling to the original
`GAME.EXE`. Where it helps, the code carries a comment at the spot.

All names below are the new names. `tools/rename_map.json` maps them back to the old placeholder names
(`C_xxxx_xxxx`, `D_xxxx`, ...), and the old address is still written in front of every renamed definition.

Confidence varies. Items marked *(unsure)* were not fully confirmed.

## Probable bugs in the original code

### Game loop, startup, disk
* `LaunchEndGame` tests `Date_D < 0`, but `Date_D` is unsigned, and it modifies `Date_D` instead of `days`. It also wraps months at 13, although the calendar has 12 months of 28 days.
* `FileErrorHandler` uses its key variable uninitialised on the disk-full and console-not-ready paths.
* The Ctrl-C branch of `DISK_confirm` treats `CurrentDrive` (a drive number) as a drive letter.
* `LoadFile` consumes `Mem_BP` even when the file did not fit.
* `PassTime`: the "vanished" item name is reset to "ring" only once per party member. A ring vanishing after a storm cloak of the same member would therefore print "cloak" (very unlikely).

### Console and status panel
* `CON_mouseOffAt` returns an uninitialised value when there is no mouse (harmless).
* `STAT_GetEquipSlot` treats `TIL_HIT` (the hit-flash tile) as a neck item. This is probably dead code or a wrong constant.
* `STAT_ComputeWeights` computes the party index of the object and never uses it.

### Map and visibility
* `MAP_LoadChunkIndexes`: the row-window shift uses `(mapId + rowDiff - 1) & 0x38`, where `- 8` was probably meant. It is harmless, because the window still contains the target.
* `VIS_FloodFill` mode 2 (peer, distance 40) indexes `DistanceTable[|dx|][|dy|]` beyond 8×8 for far squares. This out-of-bounds read only affects `AreaLight`.
* The light-on-wall test in visible.c checks the south square twice. One of the two was probably meant to be another neighbour.

### Objects
* `OBJ_CompareOrder`: in the `root0 == root1` block both tests look at the same object, so the second branch is dead code.
* `OBJ_GetCount` passes `ObjShapeType[obj]` (frame bits included) to `QuanType`, so a stackable object with a non-zero frame counts as 1.
* `OBJ_RelinkNPCs`: if temporary slot 0xFF is in use, the free-list link of the previous slot becomes 0x100 (the list head). This is a latent bug.
* `OBJ_WriteBlockFile`, disk-full "S" case: the 2-byte `.tmp` file of the current area stays on disk, but its `ObjBlkTmpPending` flag is cleared.

### Magic
* `SPELL_CastFromBook` with a map target: `TakeReagents` receives `Selection.obj` (an object number) as the page-line index, which decrements `SpellPageCounts[Selection.obj]` out of bounds. The reagent check can also read `SpellPageCounts[-1]`.
* `SPELL_DoEnchant`: `Selection.obj == -1 && Selection.obj >= 0x100` can never be true; `||` was probably meant.
* `SPELL_RefreshPage`: `SpellPageNav[0] = prevCircle - SpellPageNav[1]` gives prevCircle-2 when `[1] == 2`.

### NPC movement and AI
* `TalkToParty` passes an object number as the `partyId` argument of `TALK_talkTo`. The old comment in `StealGold` blamed the wrong place.
* `NPC_TakeTurn` sets `NPCMode` to `AI_LOITER` before testing it, so `AI_THIEF` never steals.
* `__DoOnPath`: the "blocked next to goal" test indexes `DirIncrX` with the raw path byte instead of `(info&3)<<1`.
* `TeleportToSchedule`: the second "near player" test uses the NPC's z instead of the destination z. When unboarding, `ClrLocal(mapObj)` runs before the `mapObj >= 0` check.
* `SpellFx[7]` and `SpellFx[10]` are tested in `SubMov` but never set anywhere.

### Combat and eggs
* `COMBAT_MoveToFrontLine` tests `EnemyFrontFirst == 0`, but the variable is -1 when there is no enemy front member, so `GetX(-1)` can be evaluated.
* `EGG_hatches` calls `FindInvType(1, OBJ_AMULET_OF_SUBMISSION)` inside the party loop with no `break`, so the Avatar is tested once per member.
* `EGG_GivePossessions` skips trapping chests for class 0 (the acid slug) through `IsMonsterAcidSlug`. This is odd, but it is what the code does.

### Object commands (objcmd.c)
* `ShowPortraitAndInventory`: `!TalkFlags[objNum] & 1` is a precedence bug; it equals `TalkFlags[objNum] == 0`.
* `UseFishingPole` uses `actorY + i - 1` where `j` was meant, so only the diagonal tiles are tested for water.
* `UseFountain` loops 10 times over the 5-entry `FountainWishes` table.
* `UseDeflatedBalloon` skips `(i,j) == (0,0)` instead of the centre tile.
* `AfterExplosion` covers y-2..y+1, not a symmetric range.
* `UseDoor`: "Unlock Magic" on a closed but unlocked door opens it; it cannot open key-locked doors.
* `UsePotion`: the blue (sleep) potion tests `MONSTER_UNDEAD` instead of `MONSTER_IMMUNE_SLEEP`, as the original comment already suspected.
* `UseHorse`: `SetType(horse, OBJ_GOLD)` before `DeleteObj` stops `DeleteObj` from removing the rear half of the multi-tile horse.

### Graphic effects and rest
* `Missiles()`: when one projectile stops, a `break` leaves the per-missile loop, so the remaining projectiles skip that step.
* `AnimHailStorm` converts the flash position in place inside the per-NPC loop, so a second NPC on the same tile gets a garbage position.
* Rest healing loops over members `0..noMeal-1`. Every member after the first one without food is therefore not healed, even if they ate.
* The night-encounter check during rest uses `!(i & 5)`, so only 2 iterations out of 8 qualify.

## Data layout quirks
* `TextWindows` (old `D_B6B5`) is declared as 9 bytes in BSS.ASM but used as `struct tClipInfo[4]` (36 bytes). Entry [2] is never initialised or used.
* `SpellPageNav` is declared with 3 elements, but `SPELL_RefreshPage` writes 4. The 4th element is `SpellNextCircle`, so the two form one 4-byte array.
* `VineFrames` is declared `[8][2]` but read as a flat 16-byte table (second index up to 3).
* `CastSoundPitch` and its sibling tables have 9 entries, but they are indexed by circle 1..8, so entry 0 is unused.
* `RestMsg` in msgs.c is defined but not declared in msgs.h and never used.
* monsdata.c redefines `NEUTRAL`/`EVIL`/`GOOD`/`CHAOTIC`, because it does not include u6.h.
* `OSI_playNote` exists both in `OSILIB/seg_33E3.c` and in `SOUND5.ASM`; the ASM version is the one linked into OSISND.LIB.
* `OSI_Started` and `TextIdle` (LOW.ASM) are written but never read.

## How the game works (useful facts)
* **Coordinates:** an object position packs x in bits 0-9, y in bits 10-19 and z in bits 20-23. For objects that are not on the map, the first word is the owner's object number.
* **Object slots:** `FreeObjHead` and `FreeMonsterHead` are the heads of the free-slot lists (temporary monsters use slots 0xE0-0xFF). `FreeObjCount` and `FreeMonsterCount` are the matching counts.
* **NPC inventories** are saved in the objblk file of the area where the NPC stands, and `OBJ_RelinkHeldItems` reattaches them after loading.
* **OWNED** (ObjStatus bit 0) really means "ok to take": taking an object without it on the surface counts as stealing.
* **Tile flags:** `TILE_DOUBLE_WIDTH` (0x80) means the object also covers the tile to its left, and `TILE_DOUBLE_HEIGHT` (0x40) means it also covers the tile above.
* **Terrain upper nibble:** on multi-tile objects it holds edge flags; on ground tiles, `SubTerrainMov` and the path finder read it as a movement cost.
* **Map:** the surface is 8×8 = 64 superchunks.
* **Levels:** `MapZ` 5 is the gargoyle land. The balloon and the sextant only work on levels 0 and 5.
* **Moons:** each moon's phase comes from a 28-day table. Its sky position (0..23) is the number of hours until it sets, and it is drawn when ≤ 14. Moongates exist at the 8 `MoonstonePos` spots. Which variable is Trammel and which is Felucca was not verified *(unsure)*.
* **Eclipse:** day 1 of every third month, or the Eclipse spell. Dawn and dusk are at hours 5 and 19.
* **Magic regeneration:** only the Avatar, Mage, Swashbuckler and Musician (long cape) object types regain magic every hour.
* **Karma:** `AddKarma` is never called; karma only ever decreases.
* **Schedules:** `SchedPointer[n]` is the first schedule entry of NPC n, and `SchedIndex[n]` is the current entry relative to it. `Schedule.time` holds the hour in bits 0-4 and the weekday in bits 5-7 (0 = daily).
* **Leader[]** has several jobs. In combat-type modes it holds the leader or target (and the melee opponent, which drives the "interference" check). In path modes it holds the path slot, and in `AI_SEEKOBJ` it holds the furniture kind.
* **Combat formation:** `PartyFrontMember`, `PartyFlankPos`/`PartyFlankNeg` and the enemy counterparts are the front and flank members of each side. They are computed in npctrack.c and are not targets.
* **Monster flags:** `MONSTER_STR_ATTACK` (0x20) means the natural attack uses STR instead of DEX to hit, and 0x40 means poisonous. 0x0800 deletes the body on death, 0x1000 leaves no corpse and fades the screen, and 0x2000 leaves blood. 0x4000 marks humans, who open doors and yell for help.
* **Monster classes:** class 0x23 (`OBJ_OPEN_CHEST`) is the mimic. `MonsterArmor` is only read by the XP formula (STR+DEX+INT+ARM+DMG+HP)/4.
* **Conversation scripts:** `OP_OWNER` patches the conversation script in memory as it runs.
* **Graphics driver:** sprite drawing goes save background (`GR_SAVE_SPRITE_BG`), draw (`GR_DRAW_SPRITE`), copy to screen (`GR_COPY_TO_SCREEN`), then restore. The mouse driver uses the same scheme: driver functions 0x0C/0x30/0x0F.
* **Music:** the musician's melody only plays through the PC speaker when no music card is present.
* **Spellbook buttons:** 0xA2..0xA5 are previous circle, previous page, next page and next circle (`CMD_SPELLBOOK_*`).

## Still unknown
* Ground tiles `TIL_001`..`TIL_0FC`, `TIL_17D`, `TIL_17F` and `TIL_1B8`..`TIL_1BB` keep their numeric names.
* Low-confidence names: `GR_ANIMATE_TILES` and `GR_CODEX_EFFECT` (driver entries 0x3F and 0x4E), `GrDriverOption` ('$' option), `AI_SCHED_SHY`, `TIL_BOOKSTAND_1` (it appears among the fire-damage tiles), and the edge cases of `PeerMap` at the world border.
