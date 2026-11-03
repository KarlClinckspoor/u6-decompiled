# Ultima VI: how the game works

How Ultima VI (version 4.5) handles conversation, time, the quest, character stats and combat, as
the decompiled code shows it. Everything here was read out of the C in `SRC/` (the readable
decompilation of `GAME.EXE`) and, for conversations and the quest, out of the game's own
conversation scripts (`CONVERSE.A`/`CONVERSE.B`, decompressed and disassembled). Each statement
names the function that implements it (`file.c: Function`), and formulas and constants are as
the code has them. Where something is inferred rather than read, it says so *(unsure)* or
*(inference)*.

Conventions: `1dN` is `OSI_rand(1, N)`, a uniform integer from 1 to N; divisions truncate.
*Actors* are objects 0..0xFF (people, monsters, vehicles; object 1 is the Avatar); objects from
0x100 up are items. The bugs found along the way are collected in [FINDINGS.md](../FINDINGS.md).

Contents
1. [Conversation](#conversation)
2. [Time](#time)
3. [Quest progression](#quest-progression)
4. [Character stats](#character-stats)
5. [Combat](#combat)

In short:
* **Conversation** is a bytecode interpreter: one script per NPC, keywords matched on the start
  of each typed word, 8 flag bits per NPC, and opcodes that give and take items, change karma,
  stats and levels, and add or remove party members.
* **Time** advances only when actors act: every actor gets its DEX in movement points per round,
  and one round is one game minute. A month is 28 days, a year 336.
* **Quest progress** has no record of its own: it is NPC flags, a handful of saved variables
  (`#Q`, karma, Gargish) and where the key objects are. The end comes when the eight moonstones,
  the two lenses and the Codex are in place and the vortex cube is used.
* **Stats**: levels rise only by meditating at the altars; max HP is 30 per level; DEX sets speed,
  to-hit and defence; INT sets magic points and spell resistance; STR sets carrying capacity.
* **Combat**: hit if `1d30 >= (DEX(defender) + 30 - attack stat) / 2`; damage `1d(weapon)` minus
  `1d(armour)`, with no strength bonus and no critical hits.

---

## Conversation

Sources: `talk.c` (entry points), `talkdr.c` (the script interpreter), `console.c` (text output and line input),
`gfx.c: LoadConversation` / `DrawPortrait`, `objcmd.c: ShowPortraitAndInventory`. Script examples come from
CONVERSE.A/B, decompressed with `port/tests/u6lzw.py` and disassembled with a small throw-away script. Offsets
such as `@07b0` are byte offsets inside one NPC's decompressed script.

### Starting a conversation

* **Talk command.** `gameloop.c` (CMD_TALK) asks for a target and calls `TALK_talkTo(Active, Selection.obj, 1)`.
  NPCs whose AI mode is `AI_CONVERSE` (0x96, set by schedules) walk up to the party and start talking on their own
  (`npctrack.c: TalkToParty`). That path passes a member's *object number* where a party index is expected
  (already listed in FINDINGS.md).
* **Who can be talked to** (`talk.c: TALK_talkTo`): object numbers 0..0xFF (the NPC slots), plus objects of type
  `OBJ_SHRINE`, `OBJ_STATUE_OF_MONDAIN/MINAX/EXODUS`. A horse (or mounted horse) is replaced by its rider
  (`COMBAT_getHead`). Anything else prints "nothing!".
* **Name shown after "Talk-":** "shrine" or "statue" for those objects. For an NPC, the real name
  (`TALK_GetCharName`) if bit 0 of `TalkFlags[npc]` is set (the "met" flag), otherwise the generic object string
  ("a man", ...).
* **Refusals** (`talkdr.c: TalkDriver`), checked in this order:
  * a party member who is not on screen: "Not on screen."
  * solo mode by anyone but the Avatar (`SoloMember` not -1 or 0): "Not in solo mode."
  * a Seance on slot 0 or a temporary NPC (≥ 0xE0): "You hear a deep moan."
  * an NPC that is dead (unless `SeanceFlag`), asleep or paralysed, in mode `AI_VIGILANTE`, `AI_FEAR`,
    `AI_RETREAT` or `AI_ARREST`, or of alignment EVIL or CHAOTIC: "No response".
  * after Armageddon (`IsArmageddon`), everyone gives "No response".
  * the speaker is the target: "Talking to yourself?"
  * slot 0, a temporary NPC that is not `OBJ_GUARD`/`OBJ_WISP`/`OBJ_GARGOYLE`/`OBJ_WINGED_GARGOYLE`, or an
    NPC with no script: "Funny, no response."
* **Shrines and statues** use their object's *quality* as the NPC number of the script (`TalkDriver`). The scripts
  are 189-191 (Mondain, Minax, Exodus), 192-199 (the eight virtue altars) and 200 (Singularity).
* **Script lookup** (`gfx.c: LoadConversation`): NPCs 0..0x62 are in CONVERSE.A and 0x63 and up in CONVERSE.B
  (index = npc - 0x63). Temporary NPCs use generic entries of CONVERSE.B: wisp 0x66 (npc 201) and guard 0x67
  (npc 202). `OBJ_GARGOYLE` maps to 0x68, but that entry of the shipped CONVERSE.B is empty, and
  `OBJ_WINGED_GARGOYLE` is not mapped at all, so a temporary gargoyle always gives "Funny, no response."
  Each file starts with a table of 4-byte offsets; each entry holds a 4-byte unpacked size, then LZW data. A size
  of 0 (or ≥ 0x2800) means the data is stored raw; NPC 125 "Ed" is stored that way. The buffer is 0x2800
  bytes (`main.c`: `TalkBuf`). The shipped files hold 97 + 103 scripts.
* **Portrait** (`ShowPortraitAndInventory`, `DrawPortrait`): a 56×64 picture, drawn unless the target is a
  shrine or the talk is a Seance. Temporary wisps, guards and gargoyles use generic portraits 0xC0-0xC2.
  The NPC's name under the portrait follows the same met rule, but the test `!TalkFlags[objNum] & 1` is the
  precedence bug already listed in FINDINGS.md.
* **Variables set before the script runs** (`talk.c: TALK_initTalk`). Strings: `$G` "milord"/"milady",
  `$N` the NPC's name (copied from the script header by `TalkDriver`), `$P` the speaker's name, `$T`
  "morning" (hour < 12), "afternoon" (< 18) or "evening". Integers: `#A` DEX, `#E` experience, `#I` INT,
  `#P` hit points and `#S` STR, all of the speaker; `#D/#M/#Y` the date, `#H` the hour, `#G` 1 if the Avatar is
  female, `#K` karma, `#L` `SpeakGargish`, `#O` PartySize-1, `#W` the NPC's `NPCMode`, and `#N`, the number of
  on-screen party members minus one (`TALK_SetNearbyPartyVar`). `$0-$9` point at the 100-byte buffers
  `TalkStrBuf` and `#0-#9` are reset to 0. Other letter variables keep their values between conversations.
  `#Q` is loaded from and saved to `IsOnQuest` (`console.c: GAME_Load/GAME_Save`), so it is the only script
  variable that survives a save. `$Z` always points at the last input (`TalkInput`), and `$Y` is set by
  `SETNAME_Y`. Variable 'X' has index 'X'-0x37 (A = 10 ... Z = 35).
* Conversation music is turned off during the talk (`MUS_PlaySong(0)`). After the talk, `SpeakGargish` is
  copied back from `#L`, but no shipped script assigns `#L`.

### Script layout

A script is a byte stream. Bytes below 0x80 are text, printed through `CON_putch`, and bytes from 0x80 up are
opcodes:

```
FF <npc#> <name> F1 <description...> F2 <main statements...>
   F7 (ASKTOP)  EF "kw1,kw2" F6 <answer> ... EF "*" F6 <answer> EE  ...
```

Lord British's script (npc 5) shows the usual pattern. MAIN sets the met flag (`SET 235;0`), prints a greeting,
and the input loop sits at `@07b0`:
`ASKTOP  KEY 'name' RES "I am Lord British, as thou knowest well." GOTO @07af  KEY 'job' RES ...  KEY 'bye' RES "May fortune favor thee." LEAVE  KEY '*' RES "I cannot help thee with that." ENDRES`.
Every answer ends with a `GOTO` back to the byte just before `ASKTOP`.

* The description follows `OP_DESC` (0xF1). `TalkDriver` prints "You see " plus that text and remembers its PC
  (`TalkDescPC`). Section bytes 0xF3-0xF5 that some scripts put after the description are not handled; they just
  end the text, because `parse_statement` stops at any byte ≥ 0xF0.
* After `OP_MAIN` (0xF2) the statements run up to the first input opcode. Then the main loop of `TalkDriver`
  repeats: read input, then run statements (`parse_statement`) until the next input opcode, `OP_LEAVE` or
  `OP_SCRIPT_MARK`. Input opcodes are only executed by that loop, so they always end a statement run.

### Input and keyword matching

| Opcode | Input |
|---|---|
| `ASKTOP` F7 | prints "you say:", reads a line of ≤ 50 printable chars (`CON_gets`) |
| `GETSTR` F9 `n B3` | reads a line into `$n` |
| `GETINT` FB `n B2` | reads a number (`ParseNumberInput`; bad input gives 0) into `#n` |
| `GETDIGIT` FC `n B2` | one digit key (Enter or Esc gives 0) into `#n`; used for "Which of you?" menus |
| `GETCHR` FA | one key into `TalkInput` |
| `GET` F8 `"yn"` | waits until one of the listed keys is pressed (e.g. `GET 'yn'`, `GET 'abc'`) |

* **Matching** (`TALK_MatchKeywords`, `str_i_compare`): an `OP_KEY` (0xEF) is followed by a list of keywords
  separated by commas, ended by `OP_RES` (0xF6). The input is split into space-separated words. A keyword
  matches when it equals the start of any word, case-insensitively, over the **whole length of the keyword**
  (`strlen(TalkKeyword)`). '?' in a keyword is a wildcard, and the keyword `*` matches anything. The engine does
  not cut keywords to 4 letters; the script writers store them that way. Of the keywords in the shipped data,
  2988 have 4 letters, 759 have 1 letter (`y`, `n`, `*`) and only 26 are longer (e.g. Iolo's `humbug`,
  Singularity's `un-or-us`). So typing "healing" matches `heal`, but typing "he" does not.
* Only the **first** matching `KEY` block of an input runs (`TalkAnswered`). Later `KEY` blocks are skipped up to
  their `ENDRES`. A block that does not match is skipped up to the next `OP_KEY`, `OP_ENDRES` or
  `OP_SCRIPT_MARK`.
* **Built-in answers:** an empty line becomes "bye" (`TALK_MatchKeywords`), so pressing Enter usually ends the
  talk. If the *whole* input is "look" (`stricmp`), `TalkDriver` replays the description itself, before any
  keyword is tried, so a script cannot answer "look". Everything else, including `name`, `job` and `bye`, is
  ordinary script keywords: 21 scripts have no `bye` key and 3 have no `*` key.
* `OP_STRSEARCH` (array, `$var`) is a second matcher. It returns the index of the first string of an array whose
  first **4** characters (or fewer, if the string is shorter) start a word of `$var`. Example: Singularity
  (npc 200) looks up `$Z` in "Control/Diligence/Passion" to pick a catacomb.

### Expressions

`parse_factor` evaluates a postfix expression on a local stack of ten longs, up to `OP_END_OF_FACTOR` (0xA7,
written `;` below) or `OP_LET_VALUE` (0xA8). The constant forms are `D3 b` (byte), `D4 w` (word) and `D2 addr`
(32-bit address inside the script). Any byte that is not an operator is pushed as its own value. So small
numbers are written raw, and 0xEB (`OP_NPC`) is pushed as itself; the operators turn it into "the NPC being
talked to" (`mk_npcnum`). Relational operators compare **unsigned** values. Once a `$` variable has been pushed,
every `EQ` in the same expression compares strings (`stricmp`). The script uses 0x8001 as its "none" value.

| Group | Opcodes (hex) and operands → result |
|---|---|
| arithmetic, logic | `GT GE LT LE NE EQ` 81-86, `ADD SUB MUL DIV OR AND` 90-95 ("Divide error from talkdr." on /0) |
| variables | `#` B2 (index → VarInt), `$` B3 (index → string pointer), `DATA` B4 (array, index → 16-bit word) |
| random | `RND` A0 (min, max) → `OSI_rand` |
| flags | `TST` AB (npc, bit) → bit of `TalkFlags[npc]` |
| inventory | `TEST_OBJ` BB (npc, type) → count; `OWNS` 9F (npc, type, quality) → 0/1; `WHOSGOT` C7 (type, quality) → the first *on-screen* party member carrying it, or 0x8001 |
| weight | `CANCARRY` 9A (npc) → STR×200 − 10×(equipment + inventory weight); `WEIGHT` 9B (type, qty) → weight in the same units |
| party | `ISINPARTY` C6 (npc) → in party, on screen, not dead, asleep or paralysed; `PARTYMEMBER` DD (index, any) → npc number, or 0 (any=0 counts on-screen members only); `ISONSCREEN` D7; `JOIN` CA / `LEAVEPARTY` CC (npc) → 0 ok, 1 in a vehicle, 2 party full / not a member |
| stats | `WOUNDED` DA, `POISONED` DC, `HORSED` 9D (npc) → 0/1; `ADDEXP` E0 (max 9999), `ADDLVL` E1, `ADDSTR/ADDINT/ADDDEX` E2-E4 (max 30): (npc, amount) → new value; an amount of 0 reads the stat |
| searches | `STRSEARCH` B7 (see above), `VALSEARCH` C3 (array, `#var`) → index or 0x8001; arrays end with `ENDSEARCH` B8 |
| objects | `SELECT_OBJECT` C0 (the player picks an item), `OWNER` C1 (patches the script in place, see FINDINGS.md), `OBJTYPE` C2 |
| stubs | `STUB_BC` BC, `STUB_BD` BD: pop 2, push 100 |

### Statements (`execute_op`)

| Opcode | Effect |
|---|---|
| `IF` A1 expr; … [`ELSE` A3 …] `ENDIF` A2 | If the value is 0, skips to the next ELSE or ENDIF. **There is no nesting**: the skip stops at the first one it finds. It skips the operands of GOTO, BYTE and WORD, but not those of `D2` addresses. |
| `GOTO` B0 addr | jump (addresses are script offsets; `TalkBase` is always 0) |
| `LET` A6 dest `:=` expr | dest is `n #`, `n $` (from another `$` or a string array element) or an array word (`D2 addr index`) |
| `SET` A4 / `CLR` A5 (npc; bit;) | set or clear bit *bit* of `TalkFlags[npc]` (8 flags per NPC, saved in OBJLIST) |
| `PRINTSTR` B5 | prints `$n` or string number *k* of an array (e.g. a random reply: `PRINTSTR @106e 0 4 RND;`) |
| `GIVEOBJ` B9 / `TAKEOBJ` BA (npc; type; quality; qty;) | `GiveObj` / `TakeObj`, which create or destroy items (gold uses the full 16-bit amount) |
| `TRANSFEROBJ` C9 (type; quality; from; to;) | moves an existing item between inventories (`FindInvType` + `InsertObj`) |
| `MOVEOBJ` C8 (obj; npc;) | puts an object in an NPC's inventory |
| `ADDKARMA` C4 / `SUBKARMA` C5 (n;) | karma +n (max 99) / −n (min 0) |
| `SETMODE` CD (npc; mode;) | sets `NPCMode`; `AI_SLEEP` (145) also makes the NPC sleep and lie down. Data uses 145, 153 (`AI_BRAWL`) and 154. |
| `HEAL` D9, `CURE` DB, `RESURRECT` D6 (npc;) | full HP; clear poison; revive the dead body that npc carries (`SPELL_ReviveNPC`) |
| `REST` 9E | innkeeper: the on-screen party heals and time passes to 5:00 |
| `GETHORSE` 9C (npc;) | puts npc on a horse (only Iolo's debug menu uses it) |
| `SETNAME_Y` D8 / `SETNAME_Y_YOU` DF (npc;) | `$Y` := npc's name ("you" for the Avatar with DF) |
| `SHOW_CONVERSE` BF (npc;) | shows another portrait, e.g. when Dupre cuts into Iolo's story |
| `SHOW_INVENTORY` BE (n;) | shows an inventory (1 = the active member) |
| `WAIT` CB | waits for a key (the text escape `*` does the same) |
| `DELAY` D0 (n;) | n×20 display ticks |
| `LEAVE` B6 | ends the conversation |

The text escapes come from `console.c: CON_putch`: `#c`/`$c` print a variable, `/` and `\` choose
singular/plural letters after the last number printed, `@word` highlights a keyword in red when help is on,
`*` waits for a key, `^c` prints a control glyph, `<`/`>` switch to runes, `` ` `` capitalizes the next letter,
and `&...&` hides text unless `SpeakGargish` is set. The `&` sections are not used in the conversation data.
Gargoyle scripts test `#L` instead: Draxinusom (npc 167) screams in Gargish and leaves if `#L == 0`.

### Ending a conversation

The main loop exits on `OP_LEAVE`, on `OP_SCRIPT_MARK` (0xFF, the end of the script), or on a structural
error: "Command for input key expected." (a KEY where input was expected) or "Logical error occured."
`TalkDriver` then clears `IsInConversation` and restores the status panel. If the member it was showing has
left, it goes back to the party view. `TALK_initTalk` restarts the area music.

### Interpreter quirks

* **Unnested IF in real data.** Beh Lem's script (npc 164, `@0f30`) has an `IF` inside another `IF`, and the
  outer body ends with a GOTO. When the outer condition is false, the skip stops at the *inner* `ENDIF`, so the
  inner `IF #6 LEAVEPARTY` is never executed. That routine was meant to send away one companion other than
  Dupre, Shamino or Iolo when the party is full. It also starts at `PARTYMEMBER(#O+1)`, which is one past the
  last member and returns 0, so `SETNAME_Y` on NPC 0 can print "Undefined." In the end Beh Lem only says he
  cannot join. This is inferred from the bytecode and `execute_op`; it was not tested in the game.
* The skip loops of `OP_IF`, `OP_ELSE` and `TALK_MatchKeywords` do not skip `D2` address operands. A 32-bit
  address that contains 0xA2/0xA3 (in an IF) or 0xEE/0xEF/0xFF (in a skipped answer) would be read as an opcode.
  Some shipped addresses do contain such bytes (Efram @043e = 0x06EF, Arvin @02ec = 0x08EE), but they all lie
  outside the skipped regions, so this is latent.
* `TalkKeyword` is 10 bytes (BSS.ASM) and the keyword copy has no length check. No shipped keyword is longer
  than 9 characters, so this is latent.
* `CON_gets(TalkInput, 50)` can store 50 characters plus the NUL, but `TalkInput` is 0x32 bytes. A 50-character
  line therefore writes a 0 into `TalkNpcName[0]`, the buffer that `$Y` may point at.
* A string `LET` from an array (`LET $0 := @addr #4 DATA;`) evaluates the index with `TALK_ResolveAddress`,
  which hands `#4 DATA;` to `parse_factor`. There `DATA` finds a single operand and reads and writes
  `lstack[-1]`, outside the stack array. The index survives and the shops work by accident. Which local
  `lstack[-1]` overlaps was not checked *(unsure)*.
* `TALK_initTalk` reads `NPCMode[objNum]` for `#W` even when objNum is a shrine or statue object (≥ 0x100), out
  of the 0x100-byte array. No shrine or statue script reads `#W`.
* `LeaveParty` compacts `Party[]` and `Names[]` but does not adjust `Active` or `SoloMember`, unlike the death
  code in `combat.c`. The consequences were not checked *(unsure)*.
* `OP_IF` returns 1 ("syntax error") when its value is −1 (0xFFFF), but `parse_statement` ignores the return
  value of `execute_op`, so such an IF simply runs its body.
* Script bug in Peer (npc 53, `@04e5`): the carry check writes `9 $` (string variable) where `9 #` was meant.
  The array index is therefore the low byte of a pointer.

---

## Time

### The clock and calendar

The world clock is five variables in the saved state block (gamestat.h, initial values in gamestat.c): `Time_M` (minute), `Time_H` (hour, 0..23), `Date_D` (day, 1..28), `Date_M` (month, 1..12) and `Date_Y` (year, `uint16`). A new game starts at 8:00 on day 4, month 7, year 161 (gamestat.c), and `LaunchEndGame` (main.c) measures the time spent in Britannia from 4-7-161.

All time passes through `PassTime(minutes)` (gameloop.c). Its rollover loop gives the calendar:

| Unit | Range | Rollover in `PassTime` |
|---|---|---|
| minute | 0..59 | `newMinutes > 59` gives +1 hour |
| hour | 0..23 | `++Time_H > 23` gives +1 day |
| day | 1..28 | `++Date_D > 28` gives +1 month |
| month | 1..12 | `++Date_M > 12` gives +1 year |

So a year has 12 × 28 = 336 days. The week has 7 days. For schedules the weekday is `(Date_D - 1) % 7 + 1` (1..7), so every month is exactly four weeks long (`UpdateSchedules`, npctrack.c). The status line shows `"%2d-%02d-%04d Wind:%2s"` as month-day-year (`DateMsg`, msgs.c). It is printed at the end of every `PassTime` call.

Ways to read the time:
* `CmdLook` (objcmd.c) prints it as 12-hour A.M./P.M. for a clock at any hour, and for a sundial only when `5 <= Time_H < 20`.
* Clock tiles on the map draw their hands with `DrawClockHands` (gameloop.c): the short hand is `ClockHourToDir[Time_H % 12]`, and the long hand is direction `(Time_M*2)/15`, giving 8 positions.
* Conversations get `$T` (morning if H < 12, afternoon if H < 18, else evening) and `#H`, `#D`, `#M`, `#Y` (talk.c).

### Game time vs. real time

There is no real-time clock. While the game waits for a key, `CON_prompt` (console.c) only animates the cursor and tiles (`IdleAnimTick`, `DisplayTick`). Nothing in the input loop calls `PassTime`. Time advances only through these paths:

* **The turn scheduler** `RunNPCTurns` (npctrack.c), called after every player command by `GameLoop` (gameloop.c). Each actor has a movement-point budget `MovePts[]` (`char`, signed). The next actor to act is the one with the highest `MovePts/DEXTE` ratio, and one with `MovePts >= DEXTE` acts at once. When no actor in the loaded area has points left, every actor gets `MovePts = (MovePts < 0) ? MovePts + DEXTE : DEXTE` (debts carry over) and `PassTime(1)` runs. **One "round" of all actors is one game minute**, and an actor with DEX *d* gets *d* points per minute. The raw `DEXTE[]` is used here, not `GetDex`, so a curse does not slow anyone down.
* Fixed jumps: rest (10 minutes per step), ship repair (5), the inn opcode, jail, resurrection, the midnight moongate and the debug key `<alt>215` (+60). See below.

There is **no separate combat clock**. In combat the same scheduler runs, so a combat round is also one minute. `PassTime(0)` is called after loading, level changes and lighting a torch, only to refresh the light, sky and moongates.

#### Movement-point costs (per action)

| Action | Cost | Where |
|---|---|---|
| Step on foot | 5 + upper nibble of `TerrainType` of the base tile and of each passable object **on the square being left**; +5 for a diagonal step | `SubTerrainMov`, `PartyAdvance`, `TryStraightMove` (npctrack.c) |
| Step on a mounted horse | half of the above (`pts >>= 1`, minimum 1) | `SubMov` |
| Ship (with wind) | `ShipWindCost[(dir - WindDir) & 7]` = {10,5,3,4,5,4,3,5} | `PartyAdvance` |
| Skiff | `SkiffCurrentCost[(dir - current) & 7]` = {3,4,5,7,10,7,5,4} | `PartyAdvance` |
| Raft / balloon | 5 | `PartyAdvance` |
| Attack command | 10 | `COMBAT_attack` (combat.c) |
| Cast a spell | `circle*3 + 10` | `SPELL_CastFromBook` (spell.c) |
| Use / Move (push) | 5 | `CmdUse`, `CmdMove` (objcmd.c) |
| Get / Drop | 3 | `CmdGet`, `CmdDrop` |
| Pass (space/Esc) | all remaining points (`MovePts = 0`) | `GameLoop` |
| NPC turn that spent nothing | 10 | `NPC_TakeTurn` |

Look and Talk spend no points, so they take no game time. `SubMov` would also halve costs for the party under `SpellFx[10]` and double them for hostiles under `SpellFx[7]`, but neither effect is ever set (already in FINDINGS). In `RunNPCTurns`, a following party member with `MovePts <= 0` sets the leader's points to 0. The party therefore moves at the pace of its slowest follower.

### Day, night and light

`PassTime` computes `AmbientLight` (0..7) every call:

* 0 during an eclipse, outside hours 5..19, or underground (`MapZ` 1..4).
* At hour 5 it is `Time_M/10 + 1` (dawn ramps 1→6). At hour 19 it is `(59 - Time_M)/10 + 1` (dusk ramps 6→1).
* It is 7 from 6:00 to 18:59.
* It is raised to at least 4 if `torchLit` (a party member has a lit torch equipped) or `SpellFx[0]` (Light/Great Light) is set.

The gargoyle land (z = 5) has normal daylight. `VIS_UpdateView` (visible.c) floods light from the party's square with `light = clamp(4 - distance + AmbientLight, 0..4)`. Then, if `AmbientLight < 7`, each visible light-emitting object (tile light level 1..3) floods its own light. Squares with light 0 are drawn as `TIL_DARKNESS`. So at night without a torch the party sees about 3 squares, and with light 4 it sees the whole 11×11 window.

Sky and time displays:
* The sun is drawn from 5 to 19 h at sky position `19 - Time_H` (`DrawSkyBar`, gfx.c).
* The sundial frame is `TIL_SUNDIAL + ((Time_H-5)>>1)` from 5 to 20 h, and the night frame otherwise (`PassTime`).
* Eggs flagged DAY hatch only from 6 to 18 h, and NIGHT eggs only from 19 to 5 h (`EGG_hatches`, egg.c).

**Eclipse:** `isEclipse = (Date_D == 1 && Date_M % 3 == 0) || SpellFx[15]` (`PassTime`, `DrawSkyBar`). So the whole first day of months 3, 6, 9 and 12 is dark (light 0, or 4 with a torch). The sun shows as `TIL_SUN_2` and the moons are hidden. `UpdateMoongates` does not test the eclipse, so moongates still open (code observation).

Light sources and their durations:

| Source | Duration | Code |
|---|---|---|
| Torch | quantity is set to 200 when lit. In each `PassTime` call, with a 50% chance (`OSI_rand(0,1)`), quantity -= minutes; at 0 "A torch burned out." Only torches **equipped** by party members burn. In normal play that averages about 400 minutes | `UseTorch` (objcmd.c), `PassTime` |
| Light spell | `SpellFx[0] = 100` minutes | `SPELL_Execute` (spell.c) |
| Great Light | `SpellFx[0] = 255` minutes. Both spells only set the light floor to 4, so Great Light just lasts longer | same |
| Infravision | `SpellFx[1] = 20` minutes: "warm" tiles are drawn on dark squares | `SPELL_Execute`, `ShowObject` (object.c) |

### Spell and item timers (`SpellFx[16]`)

Every `PassTime(n)` subtracts `n` from each non-zero `SpellFx[i]`, clamped at 0. These are the only timed spell effects:

| Index | Effect | Set to (minutes) |
|---|---|---|
| 0 | Light / Great Light | 100 / 255 |
| 1 | Infravision | 20 |
| 13 | Negate Magic (blocks casting: `SPELL_CastFromBook`) | 20; Storm Cloak readied: 20 (`STAT_Ready`), then forced back to 1 by each `PassTime` while it is worn |
| 14 | Time Stop | 10 |
| 15 | Eclipse spell | 20 |
| 7, 10 | tested in `SubMov`, never set | — |

**Time Stop** (`SpellFx[14]`): `PassTime` subtracts the minutes from it and **returns at once**, so the clock, the other timers, statuses, torches and schedules are all frozen until it runs out. In `RunNPCTurns` each round still calls `PassTime(1)`, so it lasts 10 rounds. `NPC_TakeTurn` makes every AI-driven actor skip its turn, party members in combat modes included. `GetDex` returns 1 for non-party actors. Animations, palette cycling and ambient sound stop as well (`AnimateTilesAndActors`, `PaletteAnimation`, `MUS_PlayAmbient`).

Protection, invisibility, curse, charm, paralysis, sleep and poison have **no timers**. They wear off at random (see Character stats → status conditions).

Other per-call and per-minute timers in `PassTime`:
* **Powder keg:** `KegFuse = 3` when lit (`UsePowderKeg` in objcmd.c, Ignite in spell.c). The keg detonates when the fuse reaches 0, so 3 minutes, and only one keg can burn at a time.
* **Magic items worn by party members:** invisibility ring, regeneration ring and storm cloak each have a 1/1001 chance per `PassTime` call to vanish (`OSI_rand(0,1000)` equal to 429, 734 or 588). The **regeneration ring** heals `+minutes` HP per call, capped at `MaxHP`.
* **Eruption:** 1/8 per call (`Erupting = 1`). It takes effect only if a volcano or fumarole is in the area (`ProcessEruption`).
* **Wind:** outdoors, 1/64 per call to pick a new `WindDir` in -1..7. Underground it is always -1 (`UpdateWind`).
* **Terrain damage** is applied to every actor once per call (`ApplyTerrainEffects`).

### Hourly events

Inside the rollover loop, once per hour crossed (`PassTime`):
* `NextSleep` and `DrunkCounter` are decremented.
* `UpdateSchedules()` runs.
* Each party member of type Avatar, Mage, Swashbuckler or long-caped Musician regains `Level` magic points, capped at `MaxMagic`.

On the first call that finds a new hour:
* `CaughtFish = 0` (at most one fish per hour, `UseFishingPole`).
* `MustRingBell = 1`: an `AI_RINGBELL` NPC rings `Time_H % 12` strokes, 12 at noon and midnight (`NPC_TakeTurn`).
* Clocks chime `MUS_Clock` = the same count (music.c).
* The sundial frame, the moons and the moongates are updated.

`DrunkCounter` goes up by 1 per drink (`UseDrink`). While it is > 3, each step has a 50% chance to go in a random direction ("Hic!", `PartyAdvance`).

### The moons and moongates

Each hour (`PassTime`):
```
TrammelPhase = MoonPhaseByDay[Date_D - 1][0];  TrammelPos = (TrammelPhase*3 + 18 - Time_H) % 24;
FeluccaPhase = MoonPhaseByDay[Date_D - 1][1];  FeluccaPos = (FeluccaPhase*3 + 20 - Time_H) % 24;
```
`MoonPhaseByDay[28][2]` (gameloop.c) gives each moon's phase 0..7 per day of the month, counting down 0,7,6,...,1. Moon 1 completes 2 cycles per 28-day month (days 1-14 and 15-28). Moon 2 completes 3 cycles (it is at phase 0 on days 1-2, 10-11, 19-20 and 28). Both are at phase 0 on day 1. The position is "hours until the moon sets". A moon is above the horizon when its position is 0..14, and negative values wrap to ≥ 15 in the `unsigned char`. So a moon of phase *p* is up from hour `3p+4` through `3p+18` (moon 1), or from `3p+6` through `3p+20` (moon 2), mod 24. It is drawn with the moonstone tile of frame = phase (`DrawSkyBar`). The header calls moon 1 Trammel and moon 2 Felucca; FINDINGS marks that assignment as unverified.

* **Moongates** (`UpdateMoongates`): at each of the 8 `MoonstonePos[phase]` spots in the loaded area, a moongate exists while `TrammelPos < 15 || FeluccaPos < 15`, and is removed otherwise. The default spots (gamestat.c) are the 8 surface gates, except phase 6 at (0x017, 0x016, z = 1). Burying a moonstone on a grass/earth tile moves the gate of its frame there (`UseMoonstone`). Taking it back clears the spot to 0,0,0 (`CmdGet`), and that gate then leads nowhere (`GateTravel`, map.c).
* **Destination** (`CheckGatesAndEntrances`, npctrack.c): the moon whose position is closer to 7 (its zenith) wins. On a tie, Trammel wins if `(Time_M < 30) == (TrammelPos < FeluccaPos)`. The winning moon's phase picks the `MoonstonePos` entry.
* **Midnight:** stepping in a moongate when `Time_H == 0 && Time_M < 10` goes to the Shrine of Spirituality (0x018, 0x01D, z = 1) and costs `PassTime(10)`.
* **Gate Travel spell:** asks for a phase 1..8 and goes to `MoonstonePos[phase-1]` if it is set (`SPELL_GateTravel`).

### NPC schedules

The schedule file holds `struct tSchedule {time, action, xyz}` (u6.h). NPC *n*'s entries are `Schedule[SchedPointer[n] .. SchedPointer[n+1]-1]`, and `SchedIndex[n]` is the current one (`GAME_Load`, `UpdateSchedules`).

* **Hourly switch** (`UpdateSchedules`): it covers NPCs 2..0xDF that are in a schedule mode (`>= AI_SCHEDULE`) or in `AI_RETREAT`, and not under player control. The game looks for an entry whose `time & 0x1f == Time_H` and whose `time >> 5` is today's weekday or 0 (every day), searching backward so the last match wins. If one matches, `SchedIndex` is set and the mode becomes `AI_FINDPATH`. If none matches, the NPC keeps its current entry. Vigilantes (`AI_VIGILANTE`) calm down every hour.
* **Getting there:** `PTH_SearchPaths` runs once per player turn. It does at most 3 off-screen teleports (`TeleportToSchedule`: refused if the NPC or the target is within 5 squares of the party, unless `AllowNPCTeleport`) and at most 3 path searches, with 8 path slots. NPCs that leave the 40×40 area are teleported or re-pathed (`RunNPCTurns`).
* **Arrival** (`StartScheduleAction`): the action becomes `NPCMode`. `AI_SLEEP` sets `ASLEEP` and the sleeping shape, in a bed if one is there, else lying down. `AI_SIT` and `AI_PLAY` look for a chair. Sleep, sit, play, eat and ring count as "arrived" anywhere.
* **Time skips:** the inn, jail, resurrection and the debug key set `AllowNPCTeleport = 1` while they skip time, so NPCs jump straight to their posts.

### Resting and camping (rest.c)

`REST_Command` refuses to rest when:
* the party is in combat;
* it is in a vehicle other than a ship;
* a building wall (wall tile with side bits) is in the 11×11 view, or the party stands on a wet tile;
* an EVIL or CHAOTIC actor is within the 39×39 area (`REST_FoesNear`);
* any non-party non-horse actor is within 5 squares;
* the game is in solo mode;
* the 3×3 squares around the leader are not all passable;
* any member is mounted.

**On a ship**, "rest" repairs the hull instead: +5 (max 100), `PassTime(5)` (`REST_RepairHull`).

The camp sequence:
1. The player gives 1..9 hours and an optional guard.
2. A campfire is lit and the party gathers around it (20 tries at most).
3. "Mealtime": while the camp song plays, members eat one item from `EdibleTypes` (14 foods), from their own pack first and then from anyone's (`TryEat`). `noMeal` records the first member without food.
4. Everyone but the guard gets the unconscious shape (`SetTypeUnconscious`). This is only a shape change: the `ASLEEP` flag is not set. The guard switches to `AI_GRAZE`.
5. The night loop runs `sleepTime*6` times. Each time the guard gets one `NPC_TakeTurn`, `PassTime(10)` runs, and when `!(i & 5)`, 1/64 chance (`OSI_rand(1,64)==1`) of a night encounter. The encounter is 1..8 wolves, snakes, giant rats or gremlins placed just outside the view, EVIL and `AI_ASSAULT` (`HasNightEncounter`). An encounter ends the rest. With a guard: "You hear something!" and everyone wakes. Without a guard: everyone gets `SetAsleep` and stays down until they wake at random or are hit.
6. Wake-up: shapes are restored and `MovePts = DEXTE`.

**Healing** happens only if `sleepTime >= 4 && NextSleep == 0` and the rest was not interrupted. Then `HealPartyMember(i, 1)` runs for party indices `0..noMeal-1` except the guard, and `NextSleep = 12`, so the next healing rest needs 12 more game hours. `HealPartyMember`:
```
missing = MaxHP - HP; if(missing == 1) missing = 2;
heal = OSI_rand(0, missing/2) + missing/2;  HP = min(HP + heal, MaxHP);  MAGIC = MaxMagic;
```
**Inns** (`OP_REST`, talkdr.c) run `HealPartyMember(i, 0)` for every on-screen member, with no food or `NextSleep` check. Then they pass whole hours until 5:00, at least one hour.

**Food otherwise:** there is no hunger clock. Eating outside rest (`UseFood`) only consumes the item. Food matters only in the rest heal.

**Other time skips:**
* Jail (`GuardArrest`): +60-minute steps until 8:00.
* Party death (`PartyResurrect`, map.c): +15-minute steps until the hour is in 8..11 or 14..17.
* Inn: see above.

---

## Quest progression

### Where the state lives

There is no "quest record" in the code. Progress is spread over:

* **`TalkFlags[0x100]`**: 8 bits per NPC, saved in OBJLIST. Bit 0 is the met flag (209 `SET 235;0` in the data;
  most scripts set it in their `name` answer, a few at the start of MAIN). The other bits are each script's
  private state. Few flags are read by other NPCs or by the C code. The C code reads two of Lord British's
  (npc 5):
  * **bit 7**: set by LB after the opening "Compendium" quiz, when he hands over the castle key
    (`GIVEOBJ 1;64;1;1` = OBJ_KEY, quality 1, to the Avatar). Until it is set, guards in `AI_ARREST` do not
    arrest (`npctrack.c: GuardArrest`), and Geoffrey (npc 7) tests it.
  * **bit 5**: set when you ask LB about `moon,gate,ston,blac,orb`. Without it, using the Orb of the Moons says
    "You can't figure out how to use it." (`objcmd.c: UseOrbOfTheMoons`).
  * The wisp script uses fixed flags of NPC 201 (bits 0, 3, 6, 7), so all wisps share them; a temporary NPC
    would otherwise use the flags of its temporary slot.
* **The gamestat block** (`gamestat.h`, saved as one block from `IsOnQuest` to `GameStateEnd`). For quest
  purposes it holds `IsOnQuest` (= script `#Q`), `KARMA` (starts at 75, `gamestat.c`), `SpeakGargish`,
  `MoonstonePos[8][3]`, the date and the Avatar's sex and portrait. Shrines, runes, lenses and the Codex have no
  fields there: they are **objects** (who carries what, where it lies), plus the flags above.
* **Objects given by scripts:** Ephemerides (npc 35) creates the Britannia lens (`OBJ_BRITANNIA_LENS_BLUE`,
  394). The Lensmaker (170) takes the broken violet lens (395) and gives the gargoyle lens (396). Draxinusom
  gives the Amulet of Submission (76). Runes come by `TRANSFEROBJ` from Ariana (Compassion, 243), Selganor
  (Sacrifice, 246) and Antonio (Humility, 249). The others are not given by scripts and are presumably found in
  the world *(inference)*. Map parts come by `TRANSFEROBJ` from Whitsaber, Arturos, Ybarra, Morchella and Homer,
  and the Gargish vocabulary (97) from Captain John.

### Shrines, runes, mantras and moonstones

* **Freeing a shrine** (`objcmd.c: UseRune`): using a rune asks "Mantra: " (up to 4 chars). The mantra must be
  the rune's own (`Mantras[]`: Ahm, Mu, Ra, Beh, Cah, Summ, Om, Lum, in the order Honesty..Humility). A moonstone
  whose frame equals the virtue must lie on one of the 8 tiles around the active member. The force field *on
  that moonstone's tile* is deleted, and every egg within 20 tiles that hatches gargoyles is destroyed
  (`DestroyGargoyleEggs`). The table `ShrineX/ShrineY` next to the mantras is unreferenced.
* **Meditating** (the scripts of npcs 192-199): the altar first comments on karma (`#K` < 40, < 80, else
  "example to all"). Then "Who will meditate?", then `ASKTOP` for the mantra.
  * The right mantra gives **+10 karma** the first time (`IF TST(235,0)==0 ADDKARMA 10; SET 235;0`). When all
  eight altars have bit 0 set, the current altar clears all eight and sets its own again, so the bonus can be
  earned again on a new round of the shrines.
  * The meditating member, if his level is ≤ 7 and his experience is at least the table value
  (0, 100, 200, 400, 800, 1600, 3200, 6400 at `@042a` of Honesty), gains a level plus the virtue's stats:
    Honesty INT+3, Compassion DEX+3, Valor STR+3, Justice INT+1 DEX+1, Sacrifice STR+1 DEX+1,
    Honor INT+1 STR+1, Spirituality all +1, Humility a level only.
  * The C code never raises `Level[]` (only `egg.c` sets it for new monsters), so levels come only from
  `ADDLVL` in scripts: the eight altars and Iolo's debug menu. The trainers (npcs 36, 58, 88, 128) only read
  the level, with `ADDLVL(member, 0)`.
* **Moonstones** (`UseMoonstone`): a moonstone can only be buried on grass/earth tiles (TIL_001-007,
  010-06F). Burying records its position in `MoonstonePos[frame]`; picking it up again (`objcmd.c`, Get)
  clears the entry. Moongates appear at those positions (see FINDINGS.md, Moons).

### The Sacred Quest and the Codex

* **Singularity** (npc 200, the pyramid altar). It refuses if `#K` < 40. It asks "For whom dost thou truly seek
  the Codex?"; accepted answers start with `both,our,peac,all,ever`, and give flag 2. It then asks for the
  Mantra of Singularity: `unorus` / `un-or-us` and the other orders of the three syllables. The correct answer
  gives `ADDKARMA 10` and **`LET #Q := 1`**. Later visits with `#Q` set get "Seek the Codex".
* **Gate keeping:** `npctrack.c: CanMoveTo` treats any object of type `OBJ_NOTHING` (0) as a wall while
  `#Q == 0`, for every mover. When the party's own step is blocked, it prints "Thou art not upon a Sacred Quest!
  Passage denied!" (once per `PartyAdvance`, through `ShowQuestBarrierMsg`). Because `#Q` is
  `IsOnQuest`, this flag is saved. Where such barriers stand was not checked *(inference: around the Codex)*.
* The statues of Mondain, Minax and Exodus teach Control, Passion and Diligence. Their closing line is chosen by
  karma (`#K / 30` indexes "Thy deeds show a lack of" ... "I admire thy deeds and thy").

### The endgame trigger

`objcmd.c: UseVortexCube`, called when the vortex cube is used and `IsArmageddon` is 0, ends the game when:

1. the cube contains moonstones of all 8 frames. Only moonstones can be put into the cube (`objcmd.c`, Move:
   "Only moonstones can go into the vortex cube.");
2. `OBJ_BRITANNIA_LENS_BLUE` lies at (0x399, 0x353, z 0) and `OBJ_GARGOYLE_LENS_VIOLET` at (0x39D, 0x353, z 0);
   `object.c: ShowObjects` draws the light-beam tiles TIL_1B8-1BB beside lenses at exactly those spots;
3. `OBJ_CODEX` lies at (0x39B, 0x353, z 0) and is within ±5 tiles of `MapX/MapY`. `MapZ` is not tested.

If so, the Codex object is deleted, "The Codex has vanished!" is printed, the game waits for a key and calls
`main.c: LaunchEndGame`. That function computes the time spent since the start date 4/7/161 as
`years = Date_Y-161`, `months = Date_M-7`, `days = Date_D-4` and runs
`execl("end.exe", "end.exe", config, years, months, days)` in a loop that never returns. `#Q` is not checked
here; the Sacred Quest only gates movement. The borrow bugs (testing `Date_D < 0` instead of days, and adding
13 months instead of 12) are in FINDINGS.md. A further effect: a negative `days` is formatted unsigned by
`CON_itoa` (e.g. "65534"), and `SRC_END/main.c` then indexes `numberWords[days]` with the `atoi` result, which
is out of bounds *(probable)*. Values of 30 or more for years would also overflow that 30-entry table.

### Party join and leave

* `talkdr.c: JoinParty` returns 1 in a vehicle and 2 if `PartySize == 8`. Otherwise it appends the NPC, copies
  at most 13 characters of its script name into `Names[]`, sets player control, turns NEUTRAL into GOOD, sets
  `AI_FOLLOW`/`AI_FRONT`, and a playing musician stops. It does not check whether the NPC is already a member;
  scripts test `ISINPARTY` first (e.g. Gwenno, npc 66, with her "Not while you're in that thing!" and
  "You have enough travelling with you already." answers for results 1 and 2).
* `LeaveParty` returns 1 in a vehicle and 2 if the NPC is not a member. The NPC goes back to `AI_WANDER`, GOOD
  becomes NEUTRAL, and **its whole inventory is dropped at its feet**, marked ok-to-take. Dupre, Shamino and
  Iolo answer `leav` with a refusal and their scripts contain no `LEAVEPARTY`. 12 scripts use `JOIN`.
* A companion who dies leaves `Party[]` (`combat.c`) but keeps player control, so `SPELL_ReviveNPC` rejoins him
  through `JoinParty`. That rejoin fails silently in a vehicle *(inference)*. Armageddon sets `PartySize = 1`.

### Karma

* **Raised only by conversations** (`OP_ADDKARMA`, max 99): +10 for each altar meditation and for Singularity,
  and smaller amounts in a few other scripts (Draxinusom +10, npc 153 +5, Pridgarm +7, Ybarra +1, ...). The C
  function `gameloop.c: AddKarma` is never called. So karma does go up, but only through the scripts.
* **Lowered by:** stealing an item without the ok-to-get flag on level 0, −1 (`objcmd.c`, Get), which also brings
  the guards; `COMBAT_RaiseAlarm` −5; killing a NEUTRAL being −5 (`combat.c`); breaking a mirror −10; the
  Pickpocket spell (`spell.c: SPELL_DoPickpocket`) −5; `SUBKARMA` in scripts (5 or 10).
* **Bug:** `SPELL_DoPickpocket` does `KARMA -= 5` directly. `KARMA` is an unsigned char and is not clamped,
  so at karma < 5 it wraps to 251-255. With that, shrines praise the Avatar, Singularity accepts him, and
  resurrection no longer costs experience (the `KARMA < 98` test fails).
* **What karma affects:** shrine, statue and Singularity texts and the Singularity < 40 refusal (all through the
  `#K` snapshot taken at the start of the talk). On resurrection, experience is multiplied by KARMA/100 if karma
  is below 98 (`spell.c: SPELL_ReviveNPC`, and `map.c` for the Avatar). Some townsfolk also test `#K`
  (Tiberius, Tara, Tobatha, Sasha, Stephanie, Gherick, Naxatilor).

### Other gates

* **Doors** (`objcmd.c: UseDoor`, `UseKeyOrLockpick`): frames 0-3 open, 4-7 closed, 8-11 locked, 12-15
  magically locked. A key opens a lock only if its quality equals the lock's *non-zero* quality; a lockpick works
  only on quality-0 locks and may break (DEX check). Scripts hand out keys of quality 1 (LB's castle key,
  re-issued if `WHOSGOT(64,1)` finds none), 11, 12 and 13. Arrest in Britain locks the cell and puts the
  quality-9 key in a desk (`GuardArrest`).
* **Eggs** (`egg.c: EGG_hatches`): no hatching after Armageddon. Day eggs (quality/10 = 1) hatch only between
  6:00 and 18:00, night eggs (= 2) only at hour ≤ 5 or ≥ 19. Gargoyles hatch peaceful (`AI_GRAZE`) if a gargoyle
  is in the party or the Avatar carries the Amulet of Submission. That amulet is forced into the neck slot and
  cannot be moved or handed over (`object.c`, `objcmd.c`).
* **Gargish:** `objcmd.c: UseGargishVocabulary` sets `SpeakGargish`. Without it, the gargoyle scripts that test
  `#L` (npcs 164-177) answer in untranslated Gargish or end the talk.
* **Debug menu (data):** in Iolo's script, typing `spam` three times then `humbug` opens a hidden menu that
  edits stats, level, experience and `#Q`, adds or subtracts karma, heals and gives a horse.

---

## Character stats

### Storage

Per-actor arrays for objects 0..0xFF are saved in `savegame\objlist` in the order of `GAME_Load` (console.c). Object 0 is the vehicle and object 1 is the Avatar. Party order is `Party[]`, with `PartySize`. `Names[16][14]` is indexed by **party index**, not object number.

| Stat | Array (type) | Notes |
|---|---|---|
| Strength | `STREN[]` (uchar) | raised only by `OP_ADDSTR`, cap 30 (talkdr.c) |
| Dexterity | `DEXTE[]` (uchar) | `OP_ADDDEX`, cap 30. 0 is forced to 1 at load (`InitNPCTracker`) |
| Intelligence | `INTEL[]` (uchar) | `OP_ADDINT`, cap 30 |
| Hit points | `HitPoints[]` (uchar) | max via `MaxHP` |
| Magic points | `MAGIC[]` (uchar) | max via `MaxMagic` |
| Level | `Level[]` (uchar) | `OP_ADDLVL`, no cap |
| Experience | `ExpPoints[]` (uint16) | cap 9999 (`AddXP`, `OP_ADDEXP`) |
| Movement points | `MovePts[]` (char) | see Time |
| Status bits | `NPCStatus[]`: PROTECTED 0x01, PARALYZED 0x02, ASLEEP 0x04, POISONED 0x08, DEAD 0x10, alignment 0x60, PLRCONTROL 0x80. `ObjStatus[]`: INVISIBLE 0x02, CHARMED 0x04, CURSED 0x40 | u6.h |
| Karma | `KARMA` (uchar, gamestat block) | starts at 75 (gamestat.c) |

**Effective values** (combat.c):
* `GetStr`, `GetDex` and `GetInt` subtract 3 when cursed, with a minimum of 1.
* `GetDex` is also 1 when asleep, and 1 for non-party actors during Time Stop.
* Carrying capacity and the displayed stats use the raw arrays.

### Derived maxima

* `MaxHP` (combat.c) is `Level*30`, between 1 and 255. Level 8 gives 240, and level 9 or more gives 255.
* `MaxMagic` (combat.c) is `2*INT` for `OBJ_AVATAR`, `INT` for `OBJ_MAGE`, `INT/2` for `OBJ_SWASHBUCKLER` and `OBJ_MUSICIAN_LONG_CAPE`, and 0 for every other type. It depends on the current object type, so a musician playing in the short-cape shape has 0.
* Spawned monsters (`EGG_generate`, egg.c):
  * STR, DEX, INT and HP come from `mkRandom(class value)`, which is `v - v/2 + rand(0..v/2) + rand(0..v/2)`.
  * `Level = (HP+29)/30`, `MAGIC = MaxMagic` and `ExpPoints = 100`.
  * Types without a class get 10/10/10, 30 HP and level 1.

### Experience and levels

* **Gaining XP:**
  * Killing an actor that attacks the party (`Is_ATKPLR`) and has a monster class gives `(Str + Dex + Int + Armor + Damage + HP of the class) >> 2` (`LoseHP`).
  * That XP goes to the killer through `AddXP`, from melee/missile hits (`__DoHit`) and from damage spells (`SPELL_MagicArrow`, `SPELL_DirectDamage`, lightning, chain bolt).
  * Conversation scripts can add XP with `OP_ADDEXP`.
* **Levels never rise automatically.** The only code that changes `Level[]` is the conversation opcode `OP_ADDLVL` (talkdr.c). The scripts that use it are the eight virtue altars (meditation, see [Quest progression](#shrines-runes-mantras-and-moonstones)) and Iolo's hidden debug menu. The same goes for `OP_ADDSTR`, `OP_ADDDEX` and `OP_ADDINT`, which each cap at 30 and are given by the altars. `UseRune` (objcmd.c) only checks the mantra to open a shrine's force field.
* `ComputeLevel` (combat.c) is **never called**. It gives `level = 1 + number of halvings of XP until <= 99`, which would be:

| XP | ≥0 | ≥100 | ≥200 | ≥400 | ≥800 | ≥1600 | ≥3200 | ≥6400 |
|---|---|---|---|---|---|---|---|---|
| Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |

* **What a level gives:**
  * max HP (+30 per level);
  * the highest spell circle that can be cast (`Level >= circle`, `SPELL_CastFromBook`; monsters with a spellbook too, `__TryHit`);
  * MP regained per hour (= Level).

### What each stat affects

| Stat | Effect | Code |
|---|---|---|
| STR | to-hit with bludgeons (spiked helm, club, mace, two-handed hammer) and for `MONSTER_STR_ATTACK` monsters | `__AttackTest` |
| STR | equipped weight ≤ STR stones (`TypeWeight + WeightEquip > STR*10` tenths: "Too heavy!"); total carried ≤ 2×STR stones (`STR*20` tenths: "The total is too heavy." / "Can't carry!") | `STAT_Ready`, `CmdGet`, `CmdMove`, `SPELL_DoPickpocket`, buckets, `OP_CANCARRY` |
| STR | break free from a web (`GetStr < 1d30` → paralyzed), from a corpser (`1d30 < STR` escapes, else 1d15 damage), wear off paralysis | `TerrainDamage`, `StruggleDraggedUnder`, `PassTime` |
| STR | resist the Poison spell: `(STR_t/2 + 30 - INT_c)/2 > 1d30` | `SPELL_ResistStrVsInt` |
| DEX | movement points per minute (raw DEXTE): speed and how often the actor acts | `RunNPCTurns` |
| DEX | to-hit with all other weapons; defence for everyone: hit if `1d30 >= (DEX_def + 30 - STAT_att)/2` | `__AttackTest` |
| DEX | dodge Magic Arrow: `(DEX_t/2 + 30 - INT_c)/2 > 1d30` | `SPELL_ResistDexVsInt` |
| INT | max MP (casters only) | `MaxMagic` |
| INT | spell resistance (sleep, charm, harm, energy wind, disable, kill...): resists if `INT_t/2 + 15 - INT_c > 1d30` | `COMBAT_ResistsSpell` |
| INT | wear-off rolls for invisibility, protection, curse and charm (see below); critically wounded NPCs with INT ≥ 5 in mobile modes flee | `PassTime`, `COMBAT_displaySeverity` |
| STR+DEX+INT | berserk AI attacks the visible opponent with the highest sum | `COMBAT_AI_Berserk` |

**Damage does not use any stat.** It is `OSI_rand(1, weapon or class damage)` (values of 255 or more pass unchanged, -1 becomes 1), minus `OSI_rand(1, protection)`. Protection is the sum of the equipped armour, -3 if cursed, +3 if protected (`__DoHit`, `__GetProtection`). Undead take `(dmg+1)>>1` except from the glass sword, magic bow and triple crossbow (`__TryHit`). No code ties carried weight to speed.

**Casting** (`SPELL_CastFromBook`) needs a readied spellbook, no Negate Magic, `Level >= circle`, `MAGIC >= circle` and reagents. It costs `circle` MP and `circle*3+10` movement points, with `circle = spell/16 + 1` (`MK_CIRCLE`, spells.h).

### Status conditions

How each condition is set, how it ends and what it does:

| Condition | Set by | Ends | Effect |
|---|---|---|---|
| **Poisoned** | hit by a poisonous monster (1/4, `__DoHit`); poison field or swamp without equipped swamp boots (`TerrainDamage`); Poison spell; green potion; trap "Poison!"/"Gas!" | **never by itself**. Red potion, Dispel Magic / Mass Dispel (`SPELL_DispelTarget`), `OP_CURE`, death/resurrection | each minute 1/8 chance to lose 1 HP, not while protected (`PassTime`). HP shown green (status.c) |
| **Asleep** | Sleep spell (INT resist; immune classes), blue potion, sleep field, schedule `AI_SLEEP`, ambushed camp | each minute 1/16 chance, **except NPCs in `AI_SLEEP`**. Any hit (`COMBAT_showHit`), orange potion, Dispel, Mass Awaken, entering a level/gate (`PartyExit`) | skips turns (`RunNPCTurns`), `GetDex` = 1 (easy to hit), corpse/sleeping shape |
| **Paralyzed** | Paralyze spell (no resistance test), web if `STR < 1d30` | each minute 1/4 × (`STR >= 1d30`). Dispel, level change | skips turns |
| **Charmed** | Charm spell (INT resist): alignment saved in NPCFlag, takes the caster's alignment, `AI_ASSAULT`; recasting on a charmed target ends it | each minute 1/8 × (`INT >= 1d30`). Dispel (`COMBAT_stopCharm`) | fights for the caster's side; a charmed member cannot be selected or controlled (`CheckActiveMember`, `GameLoop`) |
| **Cursed** | Curse, Mass Curse | each minute 1/16 × (`INT >= 1d30`, INT already -3) | -3 STR/DEX/INT, -3 protection |
| **Protected** | Protection, Mass Protect, purple potion | each minute 1/64 × (`INT <= 1d30`): high INT keeps it longer | +3 protection; immune to web, poison/swamp, fire/lava, force field/spikes/vines, traps, sleep field; poison does not tick |
| **Invisible** | Invisibility, Mass Invis, black potion, ring readied (`STAT_Ready`) | each minute 1/64 × (`INT <= 1d30`), never while a ring is worn. Removing the ring, the ring vanishing, death | cannot be seen or targeted, except party members seeing each other (`COMBAT_canSee`) |
| **Dead** | HP reaches 0 (`LoseHP`) | resurrection | see below |

"Each minute" means one roll per minute of the `minutes` passed, for all 256 actor slots wherever they are (`PassTime`). A rest of 10 minutes therefore gets 10 rolls.

### Death and resurrection

`LoseHP` (combat.c) handles damage:
* Lord British is reset to 255 HP.
* Skiff or raft damage goes to a random member.
* A ship turns into a raft with 10 HP.
* For anyone else, when `HP <= damage`: HP goes to 0, `DEAD` is set, killing a NEUTRAL actor costs 5 karma, XP is awarded (above), the shape becomes the corpse and a dead body object is made (`OBJ_MakeDeadBody`). A dead companion is removed from `Party[]`.

When the **Avatar (object 1)** dies, `CheckActiveMember` calls `PartyDeath` (requiem, "KAL LOR"), then `PartyResurrect` (map.c, also the Help spell):
* Every dead PLRCONTROL actor is revived from its body. The Avatar gets `MAGIC = INT`. Others go through `SPELL_ReviveNPC` and rejoin.
* Experience is cut to `XP*KARMA/100` if `KARMA < 98`.
* All members: awake, GOOD alignment, curse/charm/invisibility and ailments cleared, `HP = MaxHP`, `MovePts = DEXTE`.
* All guards are deleted and every `SpellFx` is cleared.
* The party is put at (0x133, 0x160, 0), and time passes until audience hours.

`SPELL_ReviveNPC` (Resurrect spell, `OP_RESURRECT`) gives 1 HP, `MAGIC = MaxMagic`, clears ailments and protection, and applies the same karma-based XP cut.

### Karma

Karma (0..99, starting at 75) is part of the quest: where it changes and what it affects is under [Quest progression → Karma](#karma). For stats it matters in one place: resurrection cuts experience to `XP*KARMA/100` when karma is below 98.

### Status panel (status.c)

* `STAT_ShowParty` lists up to 5 members with their HP: green if poisoned, red if < 10.
* `STAT_ShowPortrait` shows STR, DEX and INT, `Magic MAGIC/MaxMagic`, `Health HP/MaxHP` (same colours) and `Lev/Exp` (`GetLevel`, `ExpPoints`).
* The inventory view shows equipped weight against STR and total weight against 2×STR, in stones. `WeightToStones` rounds tenths, and anything above 0 shows at least 1.
* `STAT_refreshComMode` prints the combat mode (COMMAND/FRONT/REAR/FLANK/BERSERK/RETREAT/ASSAULT).

---

## Combat

Everything below comes from the readable decompilation in `SRC/`. Most of it is in `combat.c`, with the turn scheduler and AI dispatcher in `npctrack.c`, creature generation in `egg.c`, the class tables in `monsdata.c` and the spell damage in `spell.c`. Every statement cites the function that implements it. `1dN` means `OSI_rand(1, N)`, a uniform integer from 1 to N. Integer division truncates, as in C.

Two terms are used throughout:

* **Actors** are objects 0..0xFF (NPCs, monsters and vehicles). Only actors have STR/DEX/INT/HP. Objects from 0x100 up are plain items.
* **Alignment** is a 2-bit field of `NPCStatus`: `NEUTRAL` 0x00, `EVIL` 0x20, `GOOD` 0x40, `CHAOTIC` 0x60 (`u6.h`). `Is_ATKPLR(i)` tests bit 0x20 ("hostile to the party": EVIL and CHAOTIC). `Is_ATKMON(i)` tests bit 0x40 ("hostile to monsters": GOOD and CHAOTIC). CHAOTIC actors are therefore hostile to both sides.

### Entering and leaving combat mode

* Combat mode is a party-wide flag, `InCombat`. It is saved in the savegame block (`gamestat.c`), and `COMBAT_begin` runs again after a load if the flag is set (`main.c`, `console.c` load path). The flag is only switched by the player's **B**egin/Break-off command (`gameloop.c`, `CMD_COMBAT` case). That command is refused in solo mode ("not in solo") and aboard a vehicle. **Nothing in the code starts combat automatically** when enemies appear. Monsters attack whether or not the party is in combat mode.
* `COMBAT_begin` (`combat.c`) does the following:
  * sets `InCombat = 1`;
  * gives every non-charmed party member its combat mode (`NPCMode = NPCComMode`) and sets `Leader[member] = member`, which means "no melee opponent";
  * gives `AI_COMMAND` to the first member who is not incapacitated or dragged under (or to the solo member). That member's previous mode is kept in `SavedNPCMode`;
  * draws the combat icon and plays song 7 ("engage.m").
* `COMBAT_breakOff` clears `InCombat` and `EnemyWarningGiven`. Living, non-charmed members get `AI_FOLLOW` (or `AI_MOTIONLESS` in solo mode), dead ones get `AI_MOTIONLESS`, and `Party[0]` (or the solo member) gets `AI_COMMAND`. Combat mode is also broken off automatically in these cases:
  * changing level by ladder or dungeon entrance (`PartyChangeLevel`, `map.c`);
  * moongate travel (`GateTravel`);
  * boarding a ship (`Board`, `npctrack.c`);
  * going to jail (`GuardArrest`);
  * the resurrection after death (`map.c`).
* While `InCombat` is set:
  * party/solo mode switching is refused (`gameloop.c`);
  * resting is refused (`rest.c`: "Not while in Combat!");
  * gates and entrances are not checked (`CheckGatesAndEntrances`);
  * a party member under AI control cannot step outside the 17×17 square centred on the controlled character (`TryStraightMove`: `CLOSE_ENOUGH0(8, MapX, MapY, new_x, new_y)`).

### Party combat modes

Each party member has a combat mode, `NPCComMode`. Clicking the mode button on a member's portrait cycles it through `AI_COMMAND`(2) → `AI_FRONT`(3) → `AI_REAR`(4) → `AI_FLANK`(5) → `AI_BERSEK`(6) → `AI_RETREAT`(7) → `AI_ASSAULT`(8) → `AI_COMMAND` (`console.c`). The button does not work for `StatusMember 0`, so the Avatar's mode cannot be changed. If combat is on, the new mode applies at once (except to the active member and to charmed members). The display names are COMMAND, FRONT, REAR, FLANK, BERSERK, RETREAT and ASSAULT (`status.c: ComModeNames`). A member who joins the party starts in `AI_FRONT` (`talkdr.c`).

A member in **COMMAND** mode is controlled by the player on its own turn. `RunNPCTurns` stops whenever the actor whose turn it is has `AI_COMMAND`, and hands control to it with `SetActive`. With several COMMAND members, the player therefore moves each of them in initiative order.

### Turn order (initiative)

The scheduler is `RunNPCTurns` (`npctrack.c`), called after every player command.

* Every actor has a pool of movement points, `MovePts`. When no eligible actor has points left, each actor gets `MovePts = DEXTE` (an actor that was in debt gets `MovePts + DEXTE`) and **one game minute passes** (`PassTime(1)`). One "round" is therefore one minute, and DEX is the number of points an actor spends per round.
* The next actor is the eligible one with the highest `MovePts/DEXTE`. An actor with `MovePts >= DEXTE` is taken at once, scanning in object-number order. Ties go to the higher DEX. To be eligible, an actor must have a shape, a mode other than `AI_MOTIONLESS`, no paralysis, no sleep (unless in `AI_SCHEDULE`), not be dead, have points left, and stand on the current level. Actors outside the 40×40 loaded area lose their points.
* `AI_FOLLOW` members never act on their own; they move with the leader (`MoveFollowers`). The leader is frozen while any follower has no points left.
* After each AI turn, `Leader[actor]` is reset to the actor itself. This clears its melee engagement (see "Ranged attacks").
* Costs (`SubMov`):

| Action | Cost |
|---|---|
| attack (AI or player; the player pays once however many weapons are used: `COMBAT_attack`) | 10 |
| step | 5 + the movement cost (upper nibble of `TerrainType`) of the base tile and of each object on it (`SubTerrainMov`) |
| wait | 5 |
| AI turn that spent nothing (`NPC_TakeTurn`) | 10 |
| follower step (`MoveFollowers`) | 5 |

* `SubMov` halves every cost for a mounted horse, and never charges less than 1. The `SpellFx[10]`/`SpellFx[7]` modifiers (halve the cost for the party, double it for hostiles) are never set anywhere.
* **Time Stop** (`SpellFx[14]`): `NPC_TakeTurn` gives no turn to any AI-driven actor, party AI members included. `GetDex` returns 1 for every actor that is not player-controlled.

### Effective attributes

(Also under [Character stats](#derived-maxima).)

* `GetStr` / `GetInt` (`combat.c`): base value, -3 when cursed, minimum 1.
* `GetDex`: base value, -3 when cursed (minimum 1). It becomes **1** when the actor is asleep, and also for every non-party actor during Time Stop.
* `MaxHP` = `Level*30`, clamped to 1..255. Spawned monsters get `Level = (HP+29)/30` (`EGG_generate`), so a monster's `MaxHP` is its spawned HP rounded up to a multiple of 30.
* `MaxMagic`: Avatar `2*INT`, Mage `INT`, Swashbuckler or long-caped Musician `INT/2`, everyone else 0.

### The attack roll

`__AttackTest(att, def, weapon)` (`combat.c`):

* A target that is an item (≥ 0x100) is always hit. The **glass sword** always hits.
* The attack stat is **STR** for bludgeons: `OBJ_SPIKED_HELM`, `OBJ_CLUB`, `OBJ_MACE` and `OBJ_TWO_HANDED_HAMMER`. A natural attack (weapon == attacker) also uses STR when the attacker's class has `MONSTER_STR_ATTACK` (all humanoids have it). Everything else uses **DEX**.
* The attack hits if

```
1d30 >= (GetDex(defender) + 30 - attackStat) / 2
```

  With equal stats the threshold is 15, so the chance is 16/30. Each 2 points of advantage add 1/30. Armour has no effect on the to-hit roll. There is no critical hit anywhere in the code.

### Damage

There is no strength bonus. `__TryHit` gets the maximum damage from `GetDamagePoints(weapon)`. For an actor (a natural attack) that is `MonsterDamage[class]`; otherwise it is `WeaponDamages[]`, with -1 (not a weapon) treated as 1. `__DoHit` then works as follows:

1. **Undead:** if the target's class has `MONSTER_UNDEAD`, the maximum becomes `(max+1)>>1` before the roll. The glass sword, triple crossbow and magic bow are exempt (`__TryHit`).
2. **Roll:** `max == -1` → 1; `1 < max < 255` → `1d(max)`; `max == 255` is a guaranteed kill that skips the roll and the armour.
3. **Armour:** an actor target subtracts `1d(protection)` when `protection > 0` and the damage is below 255. Protection is the sum of `GetArmorPoints` over every **equipped** item, -3 if cursed and +3 if under Protection (`__GetProtection`). `MonsterArmor[]` is **not** used here. A monster's only protection is the armour it was spawned with and actually wears.
4. If the result is ≤ 0, the attack "grazes" and does no damage. Otherwise:
   * an adjacent attacker engages the defender in melee: `Leader[def] = att`, if the defender's mode is between `AI_COMMAND` and `AI_PATROL_EW`;
   * `LoseHP(def, dmg)` is applied, and the attacker gains the XP it returns (`AddXP`).
5. On an actor that survived, the special attacks follow. They are applied **even when the hit was only a graze**, because the block does not test the damage (see "Monster special attacks"). After that come the wound message (`COMBAT_displaySeverity`, only when damage > 0) and the neutral-provocation check (`COMBAT_ProvokeNeutral`, only when the defender is the selected target).

`LoseHP` (`combat.c`) also covers these cases:

* Lord British's HP is reset to 255, so he cannot be hurt.
* Damage to a skiff or raft is passed on to a random party member.
* A ship reduced to 0 becomes a raft with 10 HP.
* An `AI_SHY` actor that takes damage switches to `AI_ASSAULT`.
* A **slime** that takes damage and survives splits: one copy (`CloneObject`, same HP) goes on a free neighbouring tile (8 random tries).
* Breakable items lose their quantity as "hit points" and break at 0. A broken mirror costs 10 karma; a trapped chest springs its trap.
* Hitting a tangle-vine segment kills its pod outright.

### Ranged attacks, reach and line of fire

* **Distance.** Range is measured with `RangeTable[|dx|][|dy|]`, roughly `max + min/2`. A distance of 8 or more on either axis counts as 9 (`COMBAT_range`). Weapons not in `RangedWeaponTypes` have range 1 (`__GetWeaponRange`). An attack beyond range fails with "Out of range" (`__TryHit` returns 2).
* **What counts as ranged** (`__TryHit`):
  * any attack on a target more than 1 tile away, except with the morning star and the halberd (reach 2, still melee);
  * every attack with the sling, throwing axe, bow, crossbow, boomerang, flask of oil, triple crossbow, zu ylem, lightning wand or fire wand, even on an adjacent target.
* **Melee interference.** A ranged attack is refused (message `InterferenceMsg`, `__TryHit` returns 1) while `COMBAT_IsEngagedInMelee` holds: the actor's `Leader[]` is an able actor within 1 tile who is not frozen by Time Stop.
* **Ammunition.** A bow or magic bow needs at least one arrow in the inventory, a crossbow or triple crossbow a bolt; otherwise "Out of arrows/bolts" is printed and nothing happens (`COMBAT_AttackWithNextWeapon`, `__TryHit`). Every shot uses 1 arrow or bolt, or 3 for the triple crossbow (`COMBAT_FireRangedWeapon`). Spent ammunition is never recovered.
* **Missed ranged shots** (`__TryHit`). The missile is redirected to a random tile in the 3×3 block around the target: `x += 1d3-2`, `y += 1d3-2`. Whatever stands there is rolled against again. The offset can be (0,0), so 1 miss in 9 re-rolls against the original target *(quirk)*.
* **Line of fire** (`Missiles`, `gfx.c`). A missile stops on a base tile with `TILE_BLOCKS_MISSILES`, except for a `TILE_WINDOW` tile adjacent to the shooter. It also stops at the first object on a tile whose own tile blocks missiles, and when it leaves `max_range`. If it stops short of its target, the hit object becomes whatever is at the stopping tile. Actors along the path are only recorded in `MissileHitList`: they do not stop a missile unless their tile has the blocking flag *(unsure: depends on the tile flags data)*. Before attacking, the AI checks the line of fire with an invisible missile (`COMBAT_Missile(..., 0)`).
* **Thrown weapons** (spear, throwing axe and dagger, at more than 1 tile; `COMBAT_FireRangedWeapon`):
  * a spare copy carried in the inventory (not equipped) is thrown first, the readied one otherwise;
  * if it hits an actor it is **destroyed**;
  * otherwise it lands at the missile's end and is flagged OK-to-take, unless it fell into deep water or the void with nothing there, in which case it is lost.
  * The **boomerang** only plays a return animation and stays in hand.
* **Special ranged items:**
  * a flask of oil is used up and leaves a fire field on a dry landing tile;
  * zu ylem is used up, does no damage, and puts the target to sleep through `SPELL_SleepTarget`;
  * a wand vanishes when `1d100 == 37` after a shot (1 in 100);
  * the triple crossbow fires up to 3 bolts (limited by the bolts carried). The two side bolts land 1 tile from the aimed tile, perpendicular to the shot (`SpreadDirTable`), and each has its own to-hit roll;
  * a ship fires a broadside perpendicular to its heading (`__TryHit`).

### Weapon and armour tables

The tables live in `combat.c`. `WeaponTypes`/`WeaponDamages` (37 entries) give the maximum damage, `RangedWeaponTypes`/`WeaponRanges` (18 entries) the range, and `ArmorTypes`/`ArmorValues` (26 entries) the protection. Some typical values:

| Weapon | Max dmg | Range | To-hit stat | Notes |
|---|---|---|---|---|
| bare hands (Avatar) | 4 (`MonsterDamage[3]`) | 1 | STR | other humanoids use their class damage (fighter 6, guard 8) |
| dagger | 6 | 3 | DEX | thrown beyond 1 tile |
| club | 8 | 1 | STR | |
| main gauche | 8 | 1 | DEX | goes in the left hand; also armour 1 |
| spear / throwing axe | 10 / 10 | 4 / 3 | DEX | thrown |
| sling | 6 | 4 | DEX | always ranged |
| mace | 15 | 1 | STR | |
| morning star | 15 | 2 | DEX | reach, melee |
| sword | 15 | 1 | DEX | |
| two-handed hammer | 20 | 1 | STR | |
| two-handed axe / sword | 20 / 20 | 1 | DEX | |
| halberd | 30 | 2 | DEX | reach, melee |
| bow / crossbow | 10 / 12 | 5 / 7 | DEX | need arrows / bolts |
| triple crossbow | 12 | 7 | DEX | 3 bolts; full damage vs undead |
| magic bow | 20 | 7 | DEX | full damage vs undead |
| boomerang | 8 | 5 | DEX | |
| glass sword | 255 | 1 | always hits | kills outright, then breaks (`GlassSwordMsg`) |
| lightning wand / fire wand | 30 / 20 | 7 / 7 | DEX | 1 in 100 to vanish |
| spiked helm / spiked shield | 4 / 4 | 1 | STR / DEX | count as extra weapons when worn |
| flask of oil | 4 | 5 | DEX | used up; fire field |
| tools (pick, shovel, hoe, knife, cleaver, pitchfork, staff…) | 4 (scythe 6, rolling pin 2) | 1 | DEX | |

Armour (`ArmorValues`): helms (leather 1, chain coif 2, iron 3, spiked 3, winged 2, brass 2, gargoyle 3, magic 5); shields (wooden 2, curved heater 3, winged 3, kite 3, spiked 2, black 2, door 4, magic 5); body armour (cloth 1, leather 2, ring 3, scale 4, chain 5, plate 7, magic 10); main gauche 1; spiked collar 2; protection ring 5.

Equipped items can be weapons and armour at once: the spiked helm, spiked shield and main gauche all appear in both tables.

**Multiple weapons.** The player's Attack command builds a list of up to 8 **equipped** items that have a damage value (`COMBAT_InitWeaponList`: `GetDamagePoints >= 0`, which includes zu ylem at 0). It then asks for one target per weapon in turn, all for one 10-point action (`COMBAT_attack`). `STAT_Ready` (`status.c`) places a one-handed weapon in the left hand when the right hand is taken, so a character can wield two weapons, and a worn spiked helm adds a third attack. With no weapon equipped the character attacks with bare hands; a ship attacks with its cannons. NPCs and monsters attack with **one** item per action (`__PickWeapon`):

* With an equipped spellbook or charge, an NPC casts with probability 1/4 × P(`rand(0, spell) < 16`), provided Negate Magic (`SpellFx[13]`) is off. Spell numbers are `(circle-1)*16 + n`, so first-circle spells (0..15) always pass the second test and higher circles pass less and less often.
* Otherwise it takes the equipped item with the highest damage whose range reaches the target, or its natural attack.
* A spellbook cast costs `circle` magic points and needs `Level >= circle`; a charge costs nothing. The spell is cast as "party member 0x10" (`__TryHit`).

### Monster classes (monsdata.c)

`GetMonsterClass(type)` (`egg.c`) is the index of the object type in `MonsterTypes` (64 classes). The parallel tables are `MonsterStr`, `MonsterDex`, `MonsterInt`, `MonsterArmor` (used only for XP), `MonsterDamage`, `MonsterHP`, `MonsterAlignment`, `MonsterFlags`, and `MonsterInvIndex`/`MonsterInvLists` for starting inventory. A spawned creature's STR, DEX, INT and HP are each `mkRandom(base)`: `rand(0,b/2) + (b - b/2) + rand(0,b/2)`, a triangular spread over [b - b/2, b + b/2] (`EGG_generate`).

Flags (`monsdata.h`):

| Flag | Value | Effect |
|---|---|---|
| `IMMUNE_SLEEP` | 0x0001 | sleep spell, sleep field |
| `IMMUNE_DEATH` | 0x0002 | kill/death spells and Fear |
| `IMMUNE_POISON` | 0x0004 | |
| `IMMUNE_FIRE` | 0x0008 | |
| `DOUBLE_FIRE_DMG` | 0x0010 | double damage from fire |
| `STR_ATTACK` | 0x0020 | natural attack hits with STR |
| `POISONOUS` | 0x0040 | |
| `UNDEAD` | 0x0080 | half weapon damage, Repel Undead |
| `FLIES` | 0x0100 | |
| `SWIMS` | 0x0200 | |
| `ETHEREAL` | 0x0400 | walks through walls |
| `NO_CORPSE` | 0x0800 | |
| `VANISHES` | 0x1000 | |
| `BLEEDS` | 0x2000 | |
| `HUMAN` | 0x4000 | |

Some abilities are hard-wired to class numbers instead of flags: acid slug 0x00, alligator 0x01, corpser 0x08, gazer 0x15, gremlin 0x17, Lord British 0x1F, slime 0x30, winged gargoyle 0x13 (gets no gold in random loot). There are no fire-breath, theft or magic-resistance flags. Breath attacks and spells come from the class's **spell list** (invisible equipped `OBJ_CHARGE` objects). The only magic resistance is the INT contest described below.

A selection of classes. XP = `(STR+DEX+INT+ARM+DMG+HP)>>2` from the base values:

| Class | STR | DEX | INT | ARM* | DMG | HP | XP | Alignment | Flags / spells |
|---|---|---|---|---|---|---|---|---|---|
| 00 acid slug | 5 | 5 | 2 | 0 | 1 | 10 | 5 | CHAOTIC | vanishes, 2×fire, iSleep; dissolves metal |
| 02 giant ant | 16 | 15 | 2 | 3 | 8 | 10 | 13 | CHAOTIC | bleeds, no corpse, STR, iSleep |
| 08 corpser | 17 | 6 | 3 | 0 | 15 | 40 | 20 | CHAOTIC | no corpse, 2×fire, iSleep; drags under |
| 0a cyclops | 27 | 8 | 7 | 4 | 20 | 40 | 26 | EVIL | STR; 2-hand hammer/sling/club, armour, chest |
| 0b daemon | 35 | 26 | 31 | 10 | 20 | 100 | 55 | EVIL | flies, undead, iFire/Death/Sleep; magic arrow, invisibility, fireball, charm |
| 0e dragon | 40 | 35 | 27 | 12 | 30 | 150 | 73 | EVIL | vanishes, flies, iFire; fireball, flame wind, lightning, chain bolt, summon |
| 0f drake | 22 | 22 | 13 | 4 | 10 | 50 | 30 | EVIL | flies, iFire; fireball, lightning |
| 13 winged gargoyle | 15 | 32 | 33 | 5 | 6 | 50 | 35 | EVIL | flies, iSleep; lightning, explosion, paralyze |
| 15 gazer | 2 | 10 | 21 | 0 | 1 | 20 | 13 | EVIL | vanishes into insects; sleep, lightning, charm |
| 16 ghost | 1 | 10 | 9 | 0 | 12 | 20 | 13 | EVIL | vanishes, ethereal, undead, iFire/Poison/Death/Sleep; invisibility, fear |
| 17 gremlin | 2 | 26 | 7 | 0 | 1 | 5 | 10 | CHAOTIC | no corpse; steals food |
| 1c hydra | 26 | 11 | 2 | 0 | 15 | 50 | 26 | CHAOTIC | vanishes, iFire/Sleep; paralyze, chain bolt, energy wind, lightning |
| 1f Lord British | 30 | 30 | 30 | 30 | 255 | 255 | 157 | GOOD | immortal; natural attack always kills |
| 21 mage | 10 | 14 | 22 | 0 | 4 | 30 | 20 | EVIL | 8 spells (magic arrow … flame wind) |
| 23 mimic (open chest) | 22 | 9 | 8 | 5 | 15 | 30 | 22 | CHAOTIC | iPoison/Sleep; poison, charm |
| 27 giant rat | 5 | 13 | 4 | 0 | 6 | 8 | 9 | CHAOTIC | poisonous, iPoison |
| 28 reaper | 21 | 20 | 16 | 4 | 20 | 30 | 27 | CHAOTIC | 2×fire, iSleep; magic arrow, charm, insect swarm, lightning |
| 2e silver serpent | 60 | 21 | 8 | 15 | 60 | 200 | 91 | CHAOTIC | vanishes, poisonous |
| 2f skeleton | 10 | 14 | 7 | 0 | 6 | 20 | 14 | EVIL | undead, iFire/Poison/Death/Sleep; sword, spear, axe, bow… |
| 30 slime | 6 | 6 | 2 | 0 | 4 | 15 | 8 | CHAOTIC | splits, poisonous, 2×fire |
| 32 giant spider | 9 | 11 | 4 | 0 | 8 | 10 | 10 | CHAOTIC | poisonous; web |
| 36 troll | 18 | 14 | 9 | 4 | 6 | 15 | 16 | EVIL | STR; axe, spear, sword, bow, club, armour, chest |
| 38 wisp | 1 | 40 | 20 | 0 | 1 | 40 | 25 | CHAOTIC | ethereal, iFire/Death/Sleep; sleep, paralyze, wisp teleport |

\* `MonsterArmor` is used only in the XP formula.

**Starting inventory** (`EGG_GivePossessions`). Each class has 4 zero-terminated lists:

* **Spells:** every entry becomes an invisible, equipped `OBJ_CHARGE`.
* **Weapons:** the n-th entry is given with probability 1/2^n. Spears, throwing axes and daggers come 6..12 at a time; a bow comes with 12..24 arrows, a crossbow with 12..24 bolts. All weapons are equipped.
* **Armour:** the n-th entry with probability 1/2^n, equipped.
* **Treasure:** the n-th entry with probability 1/2^n. The entries mean:
  * `OBJ_OPEN_CHEST`: a closed chest, trapped 1 time in 2 (never for the acid slug); 3/4 1..100 gold; 3/4 1..10 food; 1/4 1..6 torches; 1/11 1..4 gems;
  * `OBJ_SPELL`: 3/4 1..100 gold (never for winged gargoyles); 1/4 1..10 food; 1/4 one random potion;
  * `OBJ_GOLD`: `1d30+1d30` gold;
  * anything else: one item.

### Monster special attacks

These are applied in `__DoHit` after any successful to-hit roll that did not kill the target, armour graze included:

* **Corpser** (class 0x08): the victim is dragged under. Each turn, `StruggleDraggedUnder` (`npctrack.c`) frees it if `1d30 < GetStr`; otherwise it takes `1d15` damage and loses its turn. While held, the victim cannot be seen or targeted (`COMBAT_canSee`). A corpser with frame 0 (buried) is also untargetable, and surfaces (frame 1) when it attacks.
* **Acid slug** (0x00): dissolves a random item of `DissolvableTypes` from the whole inventory: metal helms, shields and armour, the main gauche, dagger, sword, cleaver and knife (`COMBAT_PickVulnerableItem`).
* **Gremlin** (0x17): deletes a random food item (`StealableFoodTypes`).
* **Poisonous** classes: poison on `rand(0,3) == 0` (1 in 4). The defender's poison immunity and Protection are **not** checked here, unlike the poison spell, poison field and green potion.
* **Sleep and paralysis** come only from spells or items: the sleep spell or zu ylem (`SPELL_SleepTarget`: class immunity, then the INT contest), and the paralyze spell, which has **no resistance roll** at all (`SPELL_CastMissile`).
* **Wisps** cast `SPELL_WISP_TELEPORT`, which moves the wisp to a free spot within 3 tiles of the player (`__TryHit`, `COMBAT_TryTeleport`).

How these conditions wear off (one roll per game minute) is in the status table under [Character stats](#status-conditions). Any hit also wakes a sleeper (`COMBAT_showHit`).

### Attack spells (damage and resistance)

The resistance tests are:

* `COMBAT_ResistsSpell`: the target resists if `INT(t)/2 + 15 - INT(c) > 1d30`;
* `SPELL_ResistDexVsInt`: the target dodges if `(DEX(t)/2 + 30 - INT(c))/2 > 1d30`;
* `SPELL_ResistStrVsInt`: the target resists if `(STR(t)/2 + 30 - INT(c))/2 > 1d30`.

Armour never reduces spell damage. XP goes to `Party[Active]`, which is the monster itself when a monster casts.

| Spell | Effect | Resistance |
|---|---|---|
| Magic Arrow | 1d10 | DEX vs INT dodge (`SPELL_MagicArrow`) |
| Harm | 1d10 | INT contest (`SPELL_DirectDamage`) |
| Disable | HP → 1 | INT contest |
| Lightning | 1d30 | none (`SPELL_CastMissile`) |
| Fireball, Explosion, Flame Wind | 1d20, ×2 for `DOUBLE_FIRE_DMG` | fire immunity only (`SPELL_FireDamage`) |
| Energy Wind | 1d30 per victim | none (`SPELL_CastWind`) |
| Chain Bolt | 1d30, then up to 6 jumps of 1d30, each to the nearest visible actor within 4 tiles of the screen centre, as long as the jump missile (range 8) gets through | none (`SPELL_ChainBolt`) |
| Kill, Mass Kill, Death Wind | all HP | `IMMUNE_DEATH`, then INT contest (`SPELL_KillTarget`) |
| Tremor | 1d30 to every EVIL, non-flying actor within 5 | none (`SPELL_DoTremor`) |
| Wing Strike | 1d20 | none |
| Hail Storm | 1 per stone | none (`gfx.c`) |
| Sleep | asleep | `IMMUNE_SLEEP`, INT contest |
| Poison | poisoned | `IMMUNE_POISON`, STR vs INT |
| Charm | takes the caster's alignment, `AI_ASSAULT` | INT contest |
| Fear | `AI_FEAR` for non-GOOD actors within 7 of the party | INT contest, then `IMMUNE_DEATH` blocks it (`SPELL_FearTarget`) |
| Repel Undead | `AI_FEAR` for `UNDEAD` actors within 7 | INT contest (`SPELL_CastAroundParty`) |

### Combat AI modes

`NPC_TakeTurn` (`npctrack.c`) dispatches on `NPCMode`. Modes below `AI_SCHEDULE` (0x80) are combat or party modes. Target selection usually goes through `COMBAT_pickOpponent`: the closest visible actor by squared Euclidean distance (random tie-break), within 8 tiles, on the same level, not dead. A retreating target only counts when within 5 tiles of the player. Who may be targeted depends on the attacker's alignment:

* NEUTRAL attacks nothing, except that `AI_VIGILANTE` attacks GOOD;
* NEUTRAL targets are always skipped;
* CHAOTIC skips other CHAOTIC actors;
* GOOD only targets `Is_ATKPLR`;
* EVIL only targets `Is_ATKMON`;
* in the fleeing modes (`AI_SHY`, `AI_SCHED_SHY`, `AI_FEAR`, `AI_RETREAT`, `AI_BRAWL`) any actor qualifies, as the one to run from.

`ComputeCombatGeometry` runs before every scheduling pass. It computes:

* the party centre (members not in FLANK or BERSERK);
* the enemy centre (`Is_ATKPLR` actors within 24 tiles);
* the axis from the party centre to the enemy centre;
* the front-most party member and the flank members of each side, from the FRONT/COMMAND actors.

| Mode | Behaviour (function) |
|---|---|
| `AI_ASSAULT` (8), `AI_VIGILANTE` | Attack the nearest opponent if it is in weapon range and in line of fire (10 points). With no line of fire, step 90° to either side at random. Out of range, step toward it. With no opponent, wander (`COMBAT_AI_Assault`). This is also the fallback of every other mode when blocked. |
| `AI_FRONT` (3) | Move onto the front line: the point on the axis at the depth of `Party[0]` plus one step (party), or of `EnemyFrontFirst` (enemies), sliding sideways while occupied. Enemies 8 or more tiles from the party centre wait; the first enemy front-liner charges the party centre 1 time in 8; otherwise assault, but a FRONT actor out of range waits instead of advancing (`COMBAT_AI_Front`, `COMBAT_MoveToFrontLine`). |
| `AI_REAR` (4) | Party members first return to within 3 tiles of the player. Step back while not behind the shallowest same-alignment FRONT actor, then stay between the two flank members. Assault when nothing needs doing or the move is blocked. With no enemies, party members walk to the player and enemies wait (`COMBAT_AI_Rear`). |
| `AI_FLANK` (5) | Target the visible opponent furthest out on the actor's own side of the axis, within 7 of the player or 5 of itself. Attack if in range and line; otherwise move to the tile one step beyond it, outward. With no enemies, party members stay within 2 of the player (`COMBAT_AI_Flank`). |
| `AI_BERSEK` (6) | Attack the visible actor within 8 with the highest raw `STR+DEX+INT`, whatever its distance from the party. A GOOD berserker only picks **EVIL** actors, an EVIL one GOOD actors, a CHAOTIC one non-chaotic, non-neutral actors; a NEUTRAL one may pick anyone. If none, assault. **Every** berserk turn costs an extra 5 points on top of the action (`COMBAT_AI_Berserk`). |
| `AI_RETREAT` (7) | Further than 7 tiles from the player: freeze. Otherwise flee directly away from the nearest actor if it is within 7 (`CLOSE_ENOUGH_S(8)`); if cornered, fight back (assault). If nothing is that close, GOOD actors head for the party centre and others wander. 1 in 4 per turn: +1 HP (up to `MaxHP`); once `HP*4/MaxHP > 0` the actor returns to `NPCComMode` (`COMBAT_AI_Retreat`). |
| `AI_SHY` (9), `AI_SCHED_SHY` (0x9A) | Same routine, but they only flee from actors within 2 (`CLOSE_ENOUGH_S(3)`); never fight back. A shy actor that is damaged turns `AI_ASSAULT` (`LoseHP`). |
| `AI_FEAR` (0x13) | Same routine, fleeing from actors within 2, but fights back when cornered. Nothing in these files ever ends `AI_FEAR` *(unsure whether something else does)*. |
| `AI_LIKE` (0x0A) | Within 2 of a member (`CLOSE_ENOUGH_S(3)`): 1 in 2 approach the party centre; otherwise aim 10 tiles in a random axis direction (1 in 2), or wait (`COMBAT_AI_Like`). |
| `AI_UNFRIENDLY` (0x0B) | Like `AI_LIKE`, but when within 2 of a member it turns `AI_ASSAULT` 1 time in 8 (`COMBAT_AI_Unfriendly`). |
| `AI_IMMOBILE` (0x0E) | Never moves. Tries 16 random tiles within ±4 for a visible actor of another, non-neutral alignment and attacks it (`COMBAT_AI_Immobile`). |
| `AI_TANGLE` (0x0D) | Tangle-vine pod. An opponent within 1: hit it, 1 in 2. Within 4: 1 in 4 grow a vine segment toward it. Otherwise 1 in 4 grow at random (`COMBAT_AI_Tangle`, `__TryGrowVine`). |
| `AI_GRAZE` (0x0C), `AI_WANDER` | Wander at random (`WanderRandomly`). |
| `AI_ARREST` (0x12) | Guard: walk to the nearest member. When adjacent, "wilt thou come quietly?" (only after Lord British's talk flag 0x80; before that the guard returns to its schedule). **Y**: jail in Britain until 8:00, lockpicks destroyed. **N**: alarm, and every arresting guard turns EVIL `AI_ASSAULT` (`GuardArrest`). |
| `AI_BRAWL` (0x99) | 1 in 4: if an opponent is within 4 of the brawl spot, walk up to it and "hit" it with the hit flash only (no damage), then step back (`NPC_TakeTurn`). |
| `AI_THIEF` (0x97) | Meant to steal 10..30 gold, noticed if `(INT(victim)+30-DEX(thief))/2 <= 1d30`, but the mode is overwritten before the test, so thieves only talk (already in FINDINGS). |
| `AI_COMMAND` (2) | Player-controlled. `AI_FOLLOW` (1): moves with the leader. |

**Morale and fleeing.** `COMBAT_displaySeverity` prints the wound level as `severity = HP*4/MaxHP`: 3 "barely", 2 "lightly", 1 "heavily", 0 "critical". On **critical**, an actor with `GetInt >= 5` switches to `AI_RETREAT`, unless its mode is BERSERK, IMMOBILE, COMMAND, FRONT, FOLLOW or MOTIONLESS. Party members in REAR, FLANK or ASSAULT mode therefore flee too. Because a monster's `MaxHP` is `Level*30` and not its spawned HP, a fresh 10-HP monster already reads as "heavily wounded" after a 1-point hit, and a monster retreating in `AI_RETREAT` can regenerate above its spawned HP up to `Level*30` *(quirk)*.

### Provoking neutrals and crime

`COMBAT_ProvokeNeutral` runs when the selected target is a NEUTRAL, non-party actor:

* If the player (`AI_COMMAND`) did damage to a `MONSTER_HUMAN`, the victim "yells for help" and `COMBAT_RaiseAlarm` runs: -5 karma; guards within 32 tiles go `AI_ARREST`; other neutral, able, mobile NPCs within 5 (`< 6`) become `AI_VIGILANTE` if they are in a schedule mode and not walking a path, `AI_FEAR` otherwise.
* A scheduled victim hit by the player becomes `AI_VIGILANTE` and stays NEUTRAL, so every further damaging hit raises the alarm again.
* Any other victim changes alignment: CHAOTIC if the attacker is GOOD, GOOD otherwise. Temporary monsters (≥ 0xE0) also switch to `AI_ASSAULT`.
* Separately, killing any NEUTRAL actor costs 5 karma (`LoseHP`).
* `UpdateSchedules` calms vigilantes back to their schedule every hour.

### Experience

`LoseHP` returns XP only when it kills an actor that is `Is_ATKPLR` (EVIL or CHAOTIC) and has a monster class:

```
xp = (MonsterStr + MonsterDex + MonsterInt + MonsterArmor + MonsterDamage + MonsterHP)[class] >> 2
```

The formula uses the class's base values, not the randomised ones. The killer gets the XP with `AddXP`, capped at 9999: the attacker in melee or ranged combat, `Party[Active]` for spells. Killing NEUTRAL or GOOD actors gives nothing. Damage from cannons and powder kegs (`objcmd.c`) gives no XP, because the return value is ignored. Levels are not raised here: `ComputeLevel` (`1 + halvings of XP until ≤ 99`) is never called, and `Level` only changes through the conversation script opcode (`talkdr.c`).

### Death, corpses and loot

`LoseHP` → `SetTypeUnconscious` → `OBJ_MakeDeadBody` (`combat.c`, `object.c`):

* HP goes to 0 and `SetDead` is applied; invisibility is cleared; the mode becomes `AI_MOTIONLESS`. A dead companion other than the Avatar is removed from the party list. The Avatar's death (object 1) triggers `PartyDeath` (requiem, "KAL LOR", `PartyResurrect`) from `CheckActiveMember`.
* **Corpse shape.** Types listed in `DeadShape` (humanoids, horse, daemon, gargoyles, troll, headless, cyclops, skeleton…) take their corpse tile, unless they are `VANISHES`. The same table supplies the sleeping shape.
* Without a corpse entry:
  * `NO_CORPSE` and `VANISHES` creatures are deleted. A deleted object spills its inventory on its tile (`DeleteObj`), so their loot lies on the ground. A `VANISHES` creature within 5 tiles also fades the screen.
  * A gazer releases insects (`EGG_generate(OBJ_INSECTS)`).
  * Slimes re-join their frames.
  * The dragon is always deleted.
  * Other creatures keep their current shape *(unsure how that body looks on screen)*.
* `BLEEDS` classes, plus the short-caped musician and `OBJ_FARMER_2`, leave a pool of blood (random frame 0..2).
* If the actor still has a shape, `OBJ_MakeDeadBody` adds a body object (quality = NPC number) holding the whole inventory, flagged OK-to-take, and moves the NPC itself to level 15 (limbo). Bodies of permanent NPCs (< 0xE0) are not local; bodies of temporary monsters stay local and are purged with them. `OBJ_ReviveFromBody` reverses this.

### Eggs (creature generation, egg.c)

An egg (`OBJ_EGG`) is an invisible map object (module header of `egg.c`, `EGG_hatches`).

The **egg's quantity** is the hatching chance in percent: it hatches if `1d100 <= quantity`.

The **egg's quality** encodes two fields:

* `quality / 10` gives the time of day:
  * 1 = day, hatches only when 6 ≤ hour ≤ 18;
  * 2 = night, hatches only when the hour is ≤ 5 or ≥ 19;
  * anything else = always.
* `quality % 10` gives the alignment: 0 keeps the class default; otherwise the alignment is `(q-1)<<5`, i.e. 1 neutral, 2 evil, 3 good, 4 chaotic.

The **embryos** are the objects inside the egg, one per creature type:

* The embryo's quantity is the maximum count. The actual count is `1d(quantity)` unless the egg's chance is exactly 100, in which case it is the full quantity. A silver serpent always comes alone.
* The embryo's quality is the creature's combat mode (`NPCMode`/`NPCComMode`, an `AI_*` value); 0 means motionless. `AI_PATROL_EW` and `AI_PATROL_NS` set the starting direction east or north.
* The `MUTANT` status bit spawns a two-headed variant of the 2-tile monsters.
* Types below `OBJ_GIANT_RAT` (except the mimic chest) are placed as plain objects.

Rules when an egg hatches:

* The first creature appears on the egg. The others are moved to a random free tile within 3 (`COMBAT_TryTeleport`: `x + 2d4 - 5`, 8 tries). For a non-local egg that is not being force-hatched, that tile must be outside the visible 11×11 square. A creature that cannot be placed is deleted.
* If a gargoyle (or winged gargoyle) is in the party, or the Avatar (object 1) carries the Amulet of Submission anywhere in his inventory, gargoyle embryos hatch in `AI_GRAZE`. Gargoyles hatched in `AI_BERSEK` get double STR, DEX and INT.
* A tangle-vine pod gets 4 vine actors around it in `AI_TANGLE`. A silver serpent gets a coiled body of `quantity+1` segments.
* After hatching, a hostile creature from a non-local egg may trigger a warning: 3 times in 4, if Shamino (NPC 3) is in the party, alive and within 5 tiles, he "hears something to the <direction>". This is given once until combat breaks off or no enemies remain.
* The egg is then marked HATCHED and invisible, whatever the roll. An egg that failed only its day/night test is not marked and can try again.

When and where eggs are processed:

* `EGG_hatchArea` runs each time the 40×40 area is rebuilt (`MAP_BuildArea`) and, with `ForceHatching`, after a party teleport (`PartyTeleport`). It processes the eggs on the current level that are more than 8 tiles from the player on either axis, local eggs, or all eggs when forced.
* `OBJ_PurgeFarLocals` (`object.c`) clears the HATCHED bit of eggs more than 20 tiles from the new area centre, so they can hatch again later. It also deletes temporary (LOCAL) monsters that far away.
* No egg hatches during Armageddon.
* `EGG_generate` creates each creature: random stats as above, `Level = (HP+29)/30`, the class alignment, `MovePts = DEX`, `ExpPoints = 100`, mode `AI_ASSAULT` until the egg overrides it, multi-tile parts (dragon 5 tiles, hydra 9 tiles, 2-tile scorpion and friends), and the possessions. Temporary monsters occupy slots 0xE0..0xFF (`AddMonster`).

### Bugs and quirks

* `EGG_hatches`: `npcMod` is set from the first embryo with a non-zero quality (or forced to `AI_GRAZE`) and is never reset, so **every later embryo in the same egg takes that first combat mode**. Only embryos after one with quality 0 read their own.
* `__DoHit`: the special attacks (drag under, acid, food theft, poison) run even when armour reduced the damage to 0. The poison attack ignores the target's `IMMUNE_POISON` flag and Protection.
* `COMBAT_AI_Assault` calls `__PickWeapon(objNum, Selection.obj)` before testing `Selection.obj >= 0`, so with no opponent it reads `ObjPos[-1]` (harmless; the result is unused).
* `COMBAT_AI_Berserk` charges an extra `SubMov(objNum, 5)` after every action, attacks included (15 points per berserk attack).
* `COMBAT_AI_Berserk`: a GOOD berserker ignores CHAOTIC enemies while any EVIL one is visible. `COMBAT_AI_Flank` lets CHAOTIC flankers target NEUTRAL actors, which `COMBAT_pickOpponent` never allows.
* Missed ranged shots can re-roll against the same target (offset (0,0), 1 in 9; `__TryHit`).
* Monster wound levels, the critical/flee threshold and retreat healing all use `MaxHP = Level*30` rather than the spawned HP (`COMBAT_displaySeverity`, `COMBAT_AI_Retreat`).
* `SPELL_FireDamage` and `TerrainDamage` read `MonsterFlags[-1]` for actors without a class (bytes 0x6000 from the end of `MonsterAlignment`). Its fire-related bits are clear, so this is harmless.
* `ComputeCombatGeometry` resets `EnemyFrontFirst`/`EnemyFlank*` each pass but never `PartyFrontMember`/`PartyFlankPos`/`PartyFlankNeg`. A side with no FRONT/COMMAND member on one flank keeps a stale value, which `COMBAT_AI_Rear` reads *(unsure whether this shows in play)*.
