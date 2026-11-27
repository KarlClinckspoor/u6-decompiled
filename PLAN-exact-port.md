# Plan: an Ultima VI port proved against DOS, as Underworld Exhumed's are

Written 2026-11-15 from a read of this repository (`portable` branch), `../Exhume`
(abedegno/Exhume at `c6f7db5`) and `../underworld-exhumed` (which pins the same commit).
Nothing here has been built or run yet. Items marked **(unverified)** are from the docs,
not from reading the tool's code, and are the first things Phase 0 has to settle.

## 1. What "exact" means there, and what we have

Underworld Exhumed's claim is not "the port replays its own recordings". It is: a session
**recorded in DOS**, from the original program's code, replays in the port with the same
game state, the same video memory and the same saved-game bytes at every checkpoint, on
every supported host, on every push. The DOS run is the ground truth; the port is graded
against it.

| | Underworld Exhumed | this port today |
|---|---|---|
| DOS build byte-identical | yes, per file and whole EXE, one command (`make check`) | yes, whole EXE, three programs, three separate scripts |
| One tree, two builds | yes | yes (`portable.h`, `compat.h`) |
| Where input is hooked | in the shared C, at the program's boundary, so DOS can record too | in port-only C (`port/sys/kbd.c`, `timer.c`, `mouse.c`, `rand.c`, `dosint.c`), under the OSI library: DOS cannot record |
| Ground truth for a session | DOS, replayed twice and required identical | none: the port is compared with itself |
| What is compared | every state section, DS:0 and the vector table, the rand seed, all of video memory and the DAC, saved games byte for byte | one FNV digest of the object, NPC and party arrays every 4096 reads |
| Goldens and a test gate | committed digests, `make test` before every push, CI on three hosts | none; CI only builds releases |
| Single routines vs the original's bytes | game routines fuzzed in Unicorn, test vectors kept | the OSI library and the drivers only (good, keep) |
| Host hazard audits | null reads, written literals, reads past tables, stack junk, promotions, struct layout vs Turbo C | promotion audit exists; the rest met as crashes |
| Stack junk | `STACK_JUNK(v)`, v read from the disassembly | `PORT_ZERO`: the port writes zeros where DOS wrote stack contents (`SRC/object.c:1344`), so that file byte is knowingly not DOS's |
| `U.EXE`, `END.EXE` | n/a | not recorded at all |

So the decompilation is already at Underworld Exhumed's standard, and the port's
*construction* is the same method. What is missing is the proof: the DOS side of the
replay, full-state comparison, goldens, and the gate that runs them.

## 2. What Exhume is

Exhume (abedegno, MIT) is three things in one repository, none of them a game:

1. **Tools** (`tools/`, Python and Node): run the original DOS toolchain headless (emu2,
   DOSBox-X, js-dos), match and verify sources against an EXE, the one-command gate, and
   for ports: compile measurement and link stubs, explicit widths, promotion audit, struct
   layout check against Turbo C, record/replay/goldens, routine fuzzing and vectors in
   Unicorn, coverage, packaging, CI templates.
2. **A runtime** (`runtime/`): the half of a port that is not the game's. `portable.h`
   (shared with the DOS build), `replay.c` (compiled by the replay DOS build *and* the
   port), the portability layer, an SDL3 platform layer, emulated VGA/EMS/PIT, Borland's
   C library, the paragraph map and far heap, a Miles AIL 2 sound library, the x86 machine
   for statically recompiled assembly, the black box.
3. **Method**: per-toolchain profiles (`profiles/`), Claude Code skills per stage
   (`skills/`), and the docs (`docs/port.md` is the port method).

It was extracted from the UW2 decompilation and holds no game data or compilers.

## 3. Why Underworld Exhumed depends on it

It is a git submodule at `exhume/` (in the local checkout it is not initialised; the pin
is `c6f7db5`, the commit `../Exhume` is at). The dependency is structural, not a
convenience:

- **The DOS build includes it.** Every shared source includes Exhume's `portable.h`,
  staged into each Turbo C compile. Without the submodule the byte-identical build does
  not compile.
- **The ports are compiled from it.** `runtime/` is built in place, never copied, and is
  in the release packages (the NOTICE says so).
- **The proof is run by it.** `make check`, `make test`, the goldens, fuzzing and CI are
  Exhume's tools reading each game's `exhume.toml`.
- **The pin is part of the claim.** "Identical to DOS" was proved with one Exhume commit;
  the submodule records which.
- **Two games share one runtime**, so a fix reaches both and neither drifts.

## 4. Can Exhume power this port?

Yes for the proof, mostly yes for the platform, no for the sound, and it needs additions
upstream. By part:

| Exhume part | Fit for Ultima VI |
|---|---|
| `replay.c`, `replay.py`, `golden.py`, `replaydos.mjs` | **The reason to adopt it.** Generic by design (bindings in `rpgame.h`). **(unverified)** how much of the runtime `replay.c` needs in the port. |
| `fuzzasm.py`, `vectors.py`, `coverage.py` | Fits: runs EXE bytes in Unicorn, as our driver tests do. `GAME.EXE` is EXEPACKed; feed it our unpacked image (`tools/exepack.py`). |
| `layoutcheck.py`, `widths.py`, `intaudit.py`, `portcheck.py` | Fit once Turbo C 2.0 runs under Exhume's DOS runner. Our `port/tools/intaudit.py` already covers one. |
| Toolchain profile | **Missing.** Profiles exist for Turbo C++ 1.01 and Borland C++ 2.0; we need Turbo C 2.0 + MASM 3.0 + TLINK. Exhume's README lists it as "would come next". |
| Per-file match/verify gate | Optional. Its rules assume VROOMM overlays; ours has none and already proves whole EXEs. Wrap our checks as the gate first. |
| Platform layer, `vga.c`, `borland.c`, game finder, black box, crash, input scripts, cheats | Fit. `vga.c` handles mode 13h. Replaces `plat_sdl2.c`, `vgadac.c`, `hostfs.c`, parts of `session.c`. |
| Paragraph map and far heap | Fits in principle; we use a flat 1 MB arena. A swap, not a need. |
| Sound library | **No.** It is Miles AIL 2; Ultima VI has Origin's own `U6*.DRV`. Exclude it, keep `adlib.c`. |
| `asm2c.py` | Not needed: our assembly is already hand-written C, tested against the original. |
| Three programs in one executable | **(unverified)** the runtime assumes one `PORT_GAME_MAIN`; our rename scheme (`U_*`, `E_*`) has to sit on top. |
| Web build, packaging, signed macOS app | Fit, late. |

## 5. The plan

Order differs from Exhume's own (platform, then replay) on purpose: we already have a
working platform, so the proof comes first and then guards the platform swap.

### Phase 0: one gate, Exhume pinned (small)
- Add Exhume as a submodule at `exhume/`, pinned. Add `exhume.toml` and a `Makefile`.
- Write the `borland-tc20` profile (command lines, name rules) so Exhume's DOS runner
  compiles with our Turbo C 2.0 and MASM 3.0.
- `make check` = `GAME.EXE`, `U.EXE`, `END.EXE` identical, by wrapping `dosbuild.py` and
  `ubuild.py`.
- Spike the two unverified items: `replay.c` built for the port without the rest of the
  runtime, and for DOS under Turbo C 2.0.
- **Exit:** `make check` green from a clean clone; both spikes answered.

### Phase 1: DOS becomes the ground truth (the crux)
- Move every outside read up into the shared C as a `portable.h` hook that is the
  original tokens under Turbo C: keys (`KBD_GetKey`), the tick counter, mouse position
  and buttons, shift state, the time seed (`OSI_timeSeed`), DOS date and time. The gate
  proves each costs no byte.
- Find what interrupts change behind the game's back (timer tick, mouse handler, the
  music driver's 60 Hz service) and route it through hooks. This is the hardest part.
- Write `rpgame.h`: the dump's sections (saved-state block, object and NPC arrays, the
  whole uninitialised data block, rand state, DS:0, the 320x200 frame and the DAC).
- Replay DOS build: the normal link with `-DREPLAY` and `replay.obj` (no overlays, so no
  modding link is needed).
- Replace `PORT_ZERO` with `STACK_JUNK(v)`, v read from our own build's disassembly.
- **Exit:** one session recorded in DOS replays in DOS twice with identical dumps.

### Phase 2: the port against DOS
- The port compiles the same `replay.c`; our `port/replay/` and `digest.c` go.
- Compare checkpoint by checkpoint; fix the port, mask only machinery, each mask with its
  reason. Expect divergences where the port invented timing: the one-tick-per-poll
  timer, the 80 ms mouse press, retrace waits, calibrated delay loops.
- **Exit:** that session identical at every checkpoint, the frame and palette included,
  and the saved game byte for byte, on Linux and Windows, gcc and clang.

### Phase 3: sessions, goldens, the test gate
- About ten sessions of ordinary play: walking and day/night, a long conversation,
  melee and ranged combat, spells, camping, a dungeon, a boat and a moongate, shops,
  save and reload. Each starts where it needs to through its `setup` (`tools/u6save.py`,
  section 21). The user's suggestions (2026-11-26):
  - **A shrine the gargoyles occupy** for combat: many enemies, melee, ranged and spells in one
    place. Each surface shrine has a gargoyle egg (gargoyles and winged gargoyles) a few tiles
    off. Chosen (by the user): the shrine at (503, 359) = (1F7h, 167h) (OBJBLKDC), with
    its egg at (1F7h, 169h). Others: (3A7h, 107h) with (3ACh, FBh); (397h, 3A7h) with
    (39Eh, 3A7h). The setup gives arms, ammunition, reagents and magic points.
    **Done: `[shrine]`, tests/replay/shrine.rec** (sha1 1241ae55..., recorded again after the drake's fix):
    - the party starts at (513, 361), 10 tiles east of the egg; loading the save builds the area,
      which hatches eggs more than 8 tiles off (EGG_hatchArea), so the gargoyles come;
    - the Avatar has a readied spellbook (Magic Arrow, Fireball) and reagents;
    - what happens: melee, Iolo's crossbow, the Avatar's Magic Arrow, the gargoyles' spells (Dupre
      paralyzed), a gargoyle killed, Dupre, Shamino and then the Avatar killed, and the
      resurrection at Lord British's castle (experience and magic lost), saved there;
    - DOS: recording = replay 1 = replay 2, saves identical;
    - port: identical to DOS in all four builds, saves identical.
  - **A jailed drake at a wizard's** for melee: the mage Rudyom's house (NPC 128, at (579, 348)).
    An egg (100%, one drake) at (569, 340), behind bars with a steel door at (571, 343)
    (OBJBLKEC). No one can stand next to the drake, so (the user's ideas) a halberd (reach 2,
    still melee: combat.c's WeaponRanges) and a cannon placed in front of the bars.
    **`[drake]`, tests/replay/drake.rec** (sha1 a5871274..., recorded with the fixed build):
    - setup: the party in the yard at (575, 350), so the egg hatches as the save loads; the
      Avatar's sword unequipped and a halberd readied; a cannon (0DDh) at (571, 340), facing west;
    - keys: to the steel door, opened, in to (571, 341), the cannon fired, six halberd attacks,
      the cannon again, passes, saved. The drake breathes fire and survives;
    - DOS: recording = replay 1 = replay 2, saves identical.
    - **The port differed** at 50 checkpoints, in 7 bytes of BSS: DirtyRect (DS:ECBC). GFX's
      missile animation adds a projectile's "last place" to it even when the projectile stopped
      before it was ever drawn (the cannon ball the bars stop), and its spr_x[]/spr_y[] are
      locals never set then: DOS read its stack's junk (167, the clamp), the port its own. It
      is an uninitialised read in the original.
    - **Fix: `STACK_JUNK_FILL`**, a new macro in SRC/u6port.h, applies Exhume's STACK_JUNK_SET
      to every element of a local array. It is nothing under Turbo C, so the gate passes and
      GAME.EXE is unchanged. In the replay build and the port, gfx.c's spr_x and spr_y start
      at 0.
    - The replay build's code grew by 4 paragraphs (RGAME.EXE `7ba129fb`; DGROUP itself is
      unmoved). `gen_dgroup.py` had DGROUP's paragraph fixed at 329Bh and now reads it from
      RGAME.MAP (329Fh); the table and the masks are otherwise the same.
    - **Every recording was made again.** GAME records farcoreleft() (RV_FARCORE), the far
      memory above the program, and 64 more bytes of code make it 64 bytes smaller: the old
      recordings gave the new build more memory than DOS has, and its replay of drake.rec
      hung. first, smoke, setup, shrine and drake were recorded again with the new build
      (their keys are KEYFEED's, so each session plays as before). Any change to the replay
      build's code size means the same.
    - **Two more places held DGROUP's old paragraph**, found when every session then differed
      in the port: gen_dgroup.py's STATIC_FUNCS gave DISK_NullCallback as 2A91h:0003 (it now
      names DISK_TEXT and the offset in it, and reads the segment from the map: 2A95h:0007), and
      exhume.toml's `[replay.segments] first_block_para` (now 329Fh). `rp.py port-check`
      now fails at once when first_block_para is not RGAME.MAP's DGROUP.
    - **Result:** first, smoke, setup, shrine and drake: each recording = replay 1 = replay 2 in
      DOS, saves identical; each identical to DOS in all four port builds, saves identical.
      port-self identical. Gate 3 of 3 (GAME.EXE unchanged); RGAME.EXE is now `7ba129fb`.
  - **More, the user's (2026-11-26):**
    - **Map transitions:** ladders down and up (the castle's corner ladders, e.g. (291, 347),
      to the dungeon under the castle; objcmd.c's UseLadder through map.c's PartyChangeLevel,
      the surface-to-level-1 coordinate scaling), and entering a dungeon by walking onto its
      entrance (npctrack.c: OBJ_MINE_SHAFT or OBJ_HOLE; the nearest, a mine shaft at (234, 409),
      dungeon 6), then back up.
      **Done:** `[ladders]` (tests/replay/ladders.rec): (291, 348, 0) -> (75, 83, 1) ->
      (291, 347, 0); `[dungeon]` (tests/replay/dungeon.rec): (234, 410, 0) -> (58, 97, 1) ->
      (234, 409, 0). Walking onto an entrance does nothing in combat mode (npctrack.c returns
      at once), which the saved game starts in: the session breaks off combat (b) first. Both:
      DOS recording = replay 1 = replay 2, the port identical to DOS in all four builds.
    - **The balloon:** its plans (10Eh) with the four parts (mammoth silk bag, balloon basket,
      cauldron, rope: objcmd.c's UseBalloonPlans), the deflated balloon set up, inflated,
      boarded, flown with the wind, landed. **Done: `[balloon]`** (tests/replay/balloon.rec):
      the plans on the ground, the parts with Dupre, the wind from the west; the Avatar
      (330, 419, 0) -> aboard (off the map, z 15) -> landed at (334, 419, 0). DOS recording =
      replay 1 = replay 2; the port identical to DOS in all four builds.
    - **Ships and skiffs, and the balloon on level 5** (the user's): **`[ships]`**
      (tests/replay/ships.rec): from Britain's pier with the deeds (a vehicle with a quality
      needs its deed), the ship boarded, sailed south and back, turned west to lie along the pier
      (a ship is left only on its sides), left; then the skiff boarded, rowed out and back, left.
      **`[balloon5]`** (tests/replay/balloon5.rec): a flood fill of level 5's walkable terrain
      finds one large region apart from the mainland, the gargoyles' shrine area around
      (127, 47), ringed by mountains; the balloon (flying: anything but wall terrain) after Wind
      Change south flies from (131, 80) over the mountains to (131, 65) and lands there. Both:
      DOS recording = replay 1 = replay 2; the port identical to DOS in all four builds. On
      level 5 the wind changes by itself (1 in 64), and the random generator also drives the
      animations, so a port trial on real time drew other numbers than DOS: only the DOS
      recording shows the real path.
    - **Every spell:** a level-8 Avatar with full magic points, a spellbook holding every spell
      and reagents for all; each cast by its incantation (c, the syllable letters of spell.c's
      SpellCode, Enter: console.c's SelectMode 3), then its target. Several sessions by circle;
      the ones that end or upset a session (Armageddon, Kill, the winds, Gate Travel, Time Stop)
      in sessions of their own.
      **Done: `[spells1]`, `[spells2]`, `[spells3]`** (circles 1-4, 5-6, 7-8; tests/replay/
      spells*.rec): every one of the 80 cast in DOS (each costs its circle in magic points; in
      spells1 Peer shows 0 because the magic regenerated in the same window). DOS recording =
      replay 1 = replay 2 for all three; spells1 and spells3 identical to DOS in all four builds.
      - **spells2 found a gcc-only port bug:** the gcc builds differed at Negate Magic, in 179
        pixels of its "static" effect (gameloop.c: `GR_PUT_PIXEL(OSI_rand(8, 167),
        OSI_rand(8, 167))`, 100 random pixels each frame while SpellFx[13] is set). GAME is
        compiled with -p (pascal calls), so Turbo C evaluates arguments left to right (the
        disassembly: the first OSI_rand's result is pushed first, the x); C leaves the order
        open, clang takes left to right (and matched), gcc right to left (x and y swapped; the
        same random draws, so the game state matched and only pixels differed). Fixed on the
        host with a temporary for the first argument, here and in egg.c's two loot calls
        (`AddInvObj(LootFoodTypes[OSI_rand(0, 2)], ..., OSI_rand(1, 10), ...)`), where gcc
        would have swapped a monster's food and its quantity. A scan of every statement with two
        OSI_rand found no other unordered pair (the rest are sums, `&&`, nested calls or separate
        statements). Gate 3 of 3, RGAME.EXE unchanged; spells2 then identical in all four builds.
      - **The scan, done** (an Opus worker, over clang's JSON AST of every SRC file with the
        port's flags, so every macro expanded): every call's arguments and every unsequenced
        operator with two side effects, or one side effect and a global read; 215 candidates,
        all reviewed by hand, **none left in GAME** beyond the three fixed (sums of two draws,
        stats against a draw, array addresses from locals, ... are harmless). clang
        -Wunsequenced and gcc -Wsequence-point: nothing. cdecl exceptions in GAME (right to
        left in DOS): CON_printf, decompress, main, MOUSE_*, the replay hooks, Turbo C's library;
        none has two order-dependent arguments. compat.h makes cdecl and pascal empty, so a
        future cdecl call with two such arguments would match DOS under gcc, not clang.
      - **U.EXE (Phase 4):** it is built without -p, so cdecl, right to left: four calls in
        SRC_U/intro.c (739, 1039, 1055, 1075) pass two rand() draws as x and y; clang swaps them
        (gcc probably matches). Cosmetic (the introduction's jitter), the same draws either way.
        Fix: the last argument into a temporary first, under `#ifndef __TURBOC__`.
  - **The endgame.** The cube and the lenses are placed, and the Codex vanishes
    (objcmd.c's UseVortexCube): all 8 moonstones in the vortex cube, the Britannia lens at
    (399h, 353h), the Gargoyle lens at (39Dh, 353h), and the Codex at (39Bh, 353h) within 5
    tiles of the party; then LaunchEndGame runs END.EXE. The setup teleports the party there
    with the cube (holding the moonstones) and both lenses; the session places them and uses
    the cube. Needs:
    - a u6save command to put items in a container;
    - the replay DOS build to close its recording before GAME's `execl` of END.EXE (a hook
      under REPLAY only, so the gate is unchanged); in DOS, the exec replaces the program
      before replay.c writes its last chunks;
    - the cutscene itself is END.EXE, proved in Phase 4. In the port, a replay already stops
      at the exec (portmain.c's on_exec).
    **`[endgame]`, tests/replay/endgame.rec:**
    - setup: the party on the carpet at (923, 856), five tiles south of the Codex; the Avatar
      with both lenses and the vortex cube holding the eight moonstones (u6save's `put`);
    - keys: to (921, 852), drop the Britannia lens north; to (925, 852), drop the Gargoyle lens
      north; use the cube (Tab to the inventory). Found by trying in the port: the inventory's
      slot cursor keeps its place between commands, so after the two drops it is on the cube;
    - **HOOK_EXEC** (SRC/hookgame.h, at the start of main.c's LaunchEndGame) calls Exhume's new
      `rp_exec`. It counts as a hook call and ends a recording or a replay there with a last
      dump, as the stop key does; the port's black box goes on, so the player sees the ending.
      It is nothing under Turbo C (gate 3 of 3). The replay build grew 32 bytes (DGROUP at
      32A1h, RGAME.EXE `b2975855`), so every recording was made again;
    - DOS: recording = replay 1 = replay 2, saves identical. All six recordings were made
      again for the grown build (endgame b7a8df86..., first e01afca1..., smoke 38457496...,
      setup bc9cc110..., shrine 484d23bc..., drake 3e4fb65e...), each recording = replay 1 =
      replay 2 in DOS, saves identical.
    - **Port:** port-check `--all` of the six new recordings (endgame, first, smoke, setup,
      shrine, drake): every one identical to DOS in all four builds, saves identical.
    - **A port bug the session found:** the port's END.EXE, run from a game, failed at once
      ("LZW_load(brit.m): no far memory") and the port exited with code 3. GAME's main takes
      the whole far heap, and `dosmem_init`, which on_exec called before END, does nothing once
      the arena exists; DOS frees the old program's memory at an exec. The new
      `dosmem_reset` zeroes the arena and empties the far heap, before END.EXE and before
      GAME.EXE (after U.EXE). The ending now plays ("A glowing portal springs from the
      floor!").

## 22. Port hazards from the code review (2026-11-26, by the coordinator)

docs/CODE_REVIEW.md (146 findings, by an Opus worker) ranks the port hazards. The first three:
- **CR-020: `AddObj` has no return,** yet about 25 callers use its value (DOS: AX from the inner
  call). Now `return AddMapObj(...)` / `return AddInvObj(...)` in object.c: the same bytes under
  Turbo C (gate 3 of 3; the replay build too, RGAME.EXE `b2975855` unchanged, so no recording
  changes).
- **CR-008: the fountain reads 10 entries of a 5-entry table.** DOS reads the next words as near
  pointers: entries 5 and 6 are DS:0100 and DS:0101, zero bytes in every dump, so an empty wish
  matches (shown in DOS: an empty wish gets "Failed", the matched-but-unlucky answer; "xyz" gets
  "No effect"). The host read 5 wild pointers. u6port.h's `FOUNTAIN_WISH(i)` is the original
  tokens under Turbo C and "" past the table on the host. Session `[fountain]`
  (tests/replay/fountain.rec): an empty wish, "xyz", three times "food"; DOS: recording =
  replay 1 = replay 2; the port identical to DOS in all four builds.
- **CR-009: a string LET from an array, `lstack[-1]`.** The disassembly of parse_factor (the
  normal build, TALKDR_TEXT 0ECB:050E) shows `sub sp,92h; push si; push di` with lstack[0] at
  [bp-92h] and sidx in SI: lstack[-1] is the caller's saved DI (low word) and SI (high word). In
  DOS, OP_DATA with one operand reads them as the array address and writes the 16-bit result over
  them, so the caller's DI and SI come back changed.
  - What that does in DOS: OP_DATA's read only goes into lstack[-1]; the loop's `sidx ++` takes
    sidx back to 0, and parse_factor returns lstack[0], the index, so the result is as if the
    operator were absent. TALK_ResolveAddress (its caller on this path) saves only SI and sets it
    from the return value; execute_op keeps varIdx in DI and pStr in memory, and reads neither
    DI nor SI after the call before its epilogue restores its own caller's. So the clobbered
    registers have no effect on this path.
  - **Port fix:** under `!__TURBOC__`, talkdr.c's `lstack` is `lstack_ + 1` of an 11-element
    array, zeroed per call, so lstack[-1] is a slot of the frame. Gate 3 of 3; RGAME.EXE
    unchanged.
  - **Session `[shop]`** (tests/replay/shop.rec): the Avatar buys a dagger from Max the
    blacksmith ("After accepting Avatar's gold, Max hands over the Dagger."). A temporary log in
    the port showed 9 one-operand OP_DATAs in it. DOS: recording = replay 1 = replay 2; the port
    identical to DOS in all four builds.
- **After the three fixes**, port-check `--all`: first, smoke, setup and shrine identical to DOS
  in all four builds. drake, endgame and fountain were not re-run (Claude Code stopped the run,
  low memory, during drake); shop was checked with the CR-009 fix in.
- **CR-001, the vortex cube takes any item: confirmed in the port.** ContainerTypes includes the
  cube, and CanPutInContainer refuses the spellbook and dead bodies but not the cube, so a drop
  or a push onto a cube on the ground puts any item in (a port trial dropped six potions in);
  only putting it in from the inventory refuses non-moonstones. UseVortexCube checks only that
  frames 0..7 are inside: with the eight potions (113h, one of each colour) in the cube and no
  moonstone, the Codex vanished and GAME ran END.EXE. Session `[potions]`: confirmed in DOS
  too (below).
- **The review's other critical claims:**
  - **CR-004, the powder keg: confirmed in DOS.** Session `[keg]` (tests/replay/keg.rec): a keg lit
    in the castle's northwest tower, then down the ladder; the surface area is unloaded and the
    keg's slot (463) is taken by a stalagmite of level 1; when the fuse runs out, Detonate(463)
    deletes the stalagmite, with an explosion where it stands. DOS recording = replay 1 = replay
    2; the port identical in all four builds. (Vanish on a lit keg gives "No effect".)
  - **CR-002, the inventory hang: confirmed in DOS and the port.** `[invhang]` (not a test
    session: it never ends): 14 items carried, a bag with two daggers; F1, Tab, the bag opened,
    a dagger readied (STAT_Ready refreshes the inventory size from the member's 14 while the
    open container is still the bag), then down past the bottom row: the game reads the third
    down and never another key, in DOS (`rp.py record --timeout 120` timed out; STATE.OUT shows
    the last key read) and in the port (no exit after 60 s, after the same key).
  - **CR-003, casting at a map target:** needs a mouse click on the map with the spellbook open.
    Checked whether the keys can do it (the user asked): the Cast command sets SelectMode 3
    (gameloop.c:1177), where console.c handles only incantation letters, Backspace, Space/Esc,
    Enter (Selection.x/y set to -1) and the arrows, which go to SpellbookKeyNav (spell lines and
    pages). Tab's free aiming is refused while the spellbook shows, and the number keys (a party
    member as target) are only read in SelectMode 1. So no key sets a map target there: mouse
    only. Open until the DOS side can script the mouse.
  - **CR-005, the healer's resurrect:** Tiberius (NPC 26, Britain) only offers it with a dead
    party member; the setup needs a dead companion and a temporary creature's corpse ahead of
    its body in the object chain. Open.
- u6save's `place` now puts the object in front of those already on its tile, as AddMapObj
  does (the first FindLoc finds, which a drop onto the tile takes as its container); `behind`
  keeps the old order, which [drake]'s stage uses (byte-identical, so drake.rec stays valid).
- Noted on the way, then corrected (an Opus worker checked it): the user's gamedata/GAME.EXE (the
  GOG copy, sha1 360a999a...) is the same program as the build (5129af96...), compressed by
  Microsoft EXEPACK (no relocations, the "RB" stub, "Packed file is corrupt"). Unpacked
  (`python tools/exepack.py gamedata/GAME.EXE build/dos/game/GAME.EXE`): the same 199,296-byte
  image (the build's other 47,770 bytes are zeros EXEPACK drops), the same 3328 relocations,
  CS:IP and SS:SP: "identical to the original". Both say "Version 4.5". The gate already compares
  that way (tools/dosbuild.py unpacks it; tools/game.sha1 is the link before EXEPACK). U.EXE and
  END.EXE are not packed and match byte for byte. The port never opens GAME.EXE. The disassembly
  used build/dos/game/GAME.EXE because it is not packed.
- **Re-checked after the three fixes** (one at a time, after the memory stops): potions, drake,
  endgame and fountain identical to DOS in all four builds, saves identical. With first, smoke,
  setup, shrine and shop above, all nine sessions pass.
- **`[potions]` in DOS** (tests/replay/potions.rec): recording = replay 1 = replay 2; "Use-Vortex
  Cube ... The Codex has vanished!" with eight potions in the cube and no moonstone. **CR-001 is
  confirmed in the original program.** The port is identical to DOS in all four builds.
- Goldens from DOS (twice, identical) committed with their recordings.
- `make test` = gate + port build + every session against its golden; pre-push hook; CI
  on Linux and Windows from an encrypted bundle (Exhume's ci-bundle).
- **Exit:** CI red on any checkpoint difference.

### Phase 4: `U.EXE` and `END.EXE` under the same proof
- Each is its own DOS program: its own hooks, replay build and sessions (the gypsy, the
  introduction, a transfer from Ultima IV and V, the ending).
- **Exit:** no part of the port is unrecorded.

### Phase 5: what the sessions do not reach
- Audits over all three programs: struct layout against Turbo C 2.0, null reads (what
  DS:0 holds under Turbo C 2.0's start-up), literals written through, ASan and
  array-bounds builds over every session, `-ftrivial-auto-var-init=pattern` for more
  stack junk.
- Fuzz targets and vectors for game routines no session pins down: combat rolls, line of
  sight, path finding, the conversation interpreter's operators.
- Fold the existing Unicorn driver tests into `make test`; publish coverage.
- **Exit:** every routine is either reached by a session, fuzzed, or listed as unverified.

### Phase 6: Exhume's runtime under the port
- SDL3 platform layer, emulated VGA with scan-out, Borland library, game finder, black
  box, crash handler; `mcga.c` writes into the emulated VGA. Sound library excluded.
- Then, if wanted: the Roland driver through libmt32emu, checked byte for byte as
  `adlib_diff.py` checks AdLib.
- **Exit:** every golden unchanged across the swap.

### Phase 7: enhancements, as Underworld Exhumed has them (the user, 2026-11-26)
Changes the original does not have, each off unless the player turns it on, in Exhume's
model (runtime/port/sys/enhance.c and enhance.h, Underworld Exhumed's docs/ENHANCEMENTS.md):
- **The registry.** A table of flags, each with a name (`--enhance NAME,...`, the settings
  file's `enhance=`, a tab of the settings screen), a line about it, a kind (presentation,
  timing, gameplay, fix) and its source. The mask is set before the game starts and never
  changes while it runs.
- **Where they live.** An enhancement is tested with `ENHANCED(i)` only in code the DOS build
  never compiles. In the shared sources that means a hook whose original tokens stay exactly the
  same under Turbo C (as STACK_JUNK_FILL does), so the gate keeps proving GAME.EXE.
- **The proof stays.** With every flag off, every session must stay identical to DOS. A
  recording made with flags on carries them (Exhume's format 5: its enhancements chunk), and
  its replay turns the same ones on. It replays in the port only, never in DOS. Each
  enhancement gets a port session of its own (port-self), recorded with the flag on and
  replayed with it on.
- **Fixes first.** Faults of the original, each a flag of kind `fix`, so the original's behaviour
  stays the default. Candidates already marked in the sources:
  - main.c LaunchEndGame: the time in Britannia shown by the ending; "days" is never corrected
    when it goes negative (it tests Date_D), and months wrap at 13;
  - objcmd.c: the fountain reads 10 wishes from a table of 5 (pointers past its end);
  - objcmd.c: a neighbour search that only ever looks at one row ("actorY + i - 1" for "+ j");
  - objcmd.c: the look at a person ("(!TalkFlags[] & 1)" for "!(TalkFlags[] & 1)");
  - objcmd.c: the cannon's turn when pushed (a misplaced parenthesis);
  - npctrack.c: an NPC's mode overwritten before it is tested;
  - disk.c: a drive number used as a letter;
  - gfx.c: the missile animation reading a never-drawn projectile's position (the drake
    session; STACK_JUNK_FILL already gives it a fixed value, a fix would not add it at all).
  
  Each needs a session that reaches it, and the fix must be shown to change only what it says.
- **A guard before burying a moonstone** (the user, 2026-11-26): ask "Bury the moonstone here?"
  in objcmd.c's UseMoonstone before it buries. It reads a key, so it changes the inputs: a flag
  like any other, carried by the recordings it is on in, and **off by default** like every
  enhancement (the user, 2026-11-26: no exception). What the code says about burying:
  - UseMoonstone buries only on grass and earth tiles (else "Cannot be buried here!"); it puts
    the stone on the ground as an ordinary map object and records `MoonstonePos[phase]`;
  - it is not irreversible: Get takes the stone back and clears its MoonstonePos
    (objcmd.c:989). Moonstones carry status 01h (OK to take), not LOCAL, so nothing deletes
    them (OBJ_PurgeFarLocals only touches NPC slots E0h..FFh);
  - while either moon is above the horizon, UpdateMoongates keeps a moongate on the stone,
    which covers it: it can be taken when both moons are down;
  - it can be found: its moongate shows where it is; entering any moongate leads to the stone
    of the phase of the moon nearer the zenith (npctrack.c's GateTravel(TrammelPhase or
    FeluccaPhase)); the Gate Travel spell goes to stone N directly. So the trouble is that the
    player loses track, and that phase's moongates lead to the burial place from then on.
- **Then the rest:** presentation (scaling filters, a wider view), timing (a faster pace, no
  waits), gameplay changes (quality of life). Each needs the same flag, session and log line.
- **Exit:** every fix is a flag, off by default; all sessions identical to DOS with all flags off.

### Phase 8: release
- Packages for Windows, Linux and macOS, the web build, with the enhancements and fixes off by
  default (replays run with all off unless the recording carries flags).

## 6. Decisions that are yours

1. **Exhume changes** (Turbo C 2.0 profile, perhaps several programs per port):
   **decided 2026-11-15: Karl's fork of Exhume.** The submodule at `exhume/` points at the
   fork; additions go on a branch there, kept rebased on abedegno's `master` so they can
   be offered upstream later. Generic runtime fixes are still worth proving on UW2
   (`examples/uw2/prove-port.sh`) before the pin moves.
2. **Phase 6 at all?** Phases 0 to 5 give the Underworld Exhumed standard of proof on the
   current SDL2 platform. Phase 6 buys shared maintenance, macOS and the web build.
3. **Where the tools run.** Exhume's tools are shell, Python and Node, used on macOS and
   Linux; here that means WSL, as the Underworld work already does.

## 7. Phase 0 results (2026-11-15)

Done by three workers, one at a time, each reviewed and its proof rerun. Nothing committed.

- **Profile.** `../Exhume`, local branch `u6-tc20`, uncommitted: `profiles/borland-tc20`
  and about 40 lines in `tools/build.py`, `config.py`, `dosrun.mjs`. Exhume's build path
  compiles and assembles our sources to objects byte-identical to `dosbuild.py`'s (3 C
  files and 4 MASM modules of GAME; 2 C files of U once the recorded source times agree).
  Not tried through Exhume: the link, huge-model files, inline-assembly C.
- **Gate.** `python tools/gate.py check` (Git Bash, 22 s) or `make check` (WSL, 90 s):
  three programs, sha1s pinned. `make test` adds the port and the Unicorn driver tests.
- **Spikes.** Experiments in the session scratchpad (`spike/RERUN.txt`).
  - `replay.c` compiles unmodified under Turbo C 2.0, with GAME's switches and U's.
  - **It does not fit as it is.** GAME's DGROUP ends at EDFA: 4,614 bytes are left for
    stack and near heap, and stock `replay.c` has 6.3 KB of `_BSS`. The link fails.
  - **Adding it moves the game's data.** Any initialised data in `replay.c`, or in a
    library module it pulls in, shifts the whole `BSS.ASM` block (by 400 bytes in the
    slimmed trial). Near pointers stored in game state then differ from the real game's.
  - `replay.c` can go into the current SDL2 port ahead of the rest of the runtime, with a
    shim of about 50 lines. A stand-in game recorded and replayed to an identical dump.
  - The stack-junk byte (`object.c:1344`) cannot be determined: interrupts write it. It
    reaches saved games. `STACK_JUNK_SET(containerPos.raw[2], 0)` compiles to the original
    bytes, so the replay build and the port both write 0.
  - **Outside reads the port does not record** (it fakes them as constants):
    `farcoreleft()` (`main.c:522`), which sets how many tile-cache pages the graphics
    driver gets and so when `CurrentDisk` changes; DOS version, drive types and current
    drive (`disk.c:181-231`); `CpuSpeed`; int 12h. `RP_SHIFT` and `RP_DATE` in the port
    are never reached by GAME.
  - The mouse is polled inside assembly that also calls back into C and redraws, so it
    needs a replay-build-only `MOUSE` module, not a C macro.
  - U.EXE reads its own code bytes as noise, so its replay build's noise differs from
    U.EXE's; it also runs game code from timer callbacks.

## 8. Phase 1, revised from the spikes

Design decision (coordinator): the replay build must not move the game's data, so the
harness keeps **nothing in DGROUP**. Its buffers and strings go in a far segment defined
by a small MASM module and reached through far pointers (Turbo C 2.0 refuses `far`
globals in the medium model, but accepts far pointers), and its file I/O goes through the
already-linked `intdos`, so no library data is pulled in. Exit test for that: the map's
`_DATA` and `BSS.ASM` offsets are the same with and without the harness.

1. Exhume's `portable.h` under the project's own; `STACK_JUNK_SET` replaces `PORT_ZERO`.
   Exit: gate. *Easy (Sonnet).*
2. Measure GAME's real stack depth in DOSBox-X with a pattern-fill probe. Exit: a number
   with margin. *Medium (Sonnet).*
3. `replay.c` for Ultima VI in the fork: far-segment storage, no DGROUP data, the seven
   binding changes the spike lists (seed location, null checksum 0DFAh, stop key,
   `KEY(arg)`, a generic recorded value, I/O macros). Exit: GAME links with data unmoved;
   UW1 and UW2 replays unchanged. *Hard (Opus).*
4. Hooks in GAME's C: keys (`console.c:564`, `main.c:235`), seed (`main.c:510`),
   `farcoreleft` (`main.c:522`), DOS and drive queries (`disk.c`). Exit: gate, no byte
   changed. *Medium (Opus).*
5. Replay-build-only `MOUSE` and `OSI_delay` modules; `CpuSpeed` recorded. Exit: the
   replay build runs in DOSBox-X. *Medium (Opus).*
6. `rpgame.h`: dump sections, masks, far-block segments. Exit: two DOS replays of a
   trivial session identical. *Hard (Opus).*
7. Replay link script, `[replay]` in `exhume.toml`, a stop key the game can see, a staged
   saved game with a character. Exit: `replay.py build` and `dos` run. *Medium (Sonnet).*
8. First real session: record in js-dos, replay twice in DOSBox-X, identical. *Hard
   (Opus).*

## 9. Phase 1 task 2: GAME's stack depth (2026-11-16)

Measured with a probe build (pattern A5 filled below the frame at the start of `main` and
again before `GameLoop`; the lowest overwritten byte read at every `CON_getch` and every
turn), headless DOSBox-X, AdLib music on, a saved game with a party of four in a fight.
The probe adds no data: `_BSSEND` is at DGROUP:EDFAh in both maps (checked in review).

- **Room.** The stack runs from 10000h down; the near heap above EDFAh holds only C0's
  copies of the arguments and environment (160 bytes under DOSBox-X, more with a larger
  environment), and the game never calls `malloc`. Usable: 4,454 bytes here.
- **Measured.** Deepest seen 1,004 bytes; every run that drew a missile 956 to 990;
  walking, time passing and conversation without one 255 to 409. The peak is `Missiles()`
  (`gfx.c`), a 616-byte frame.
- **Not reached by any run:** spell casting, rest, a town, save and load, explosion chains.
  The measured figure is therefore a floor, not a ceiling.
- **Static.** Longest simple call path 2,134 bytes (not feasible as a whole; plausible
  chains about 1,600 to 1,900). Recursion exists (a 69-function component; the cheapest
  cycle is 44 bytes a level, the one through `Missiles` 878) and its depth is not bounded.
  203 indirect call sites and the drivers' interrupt handlers are not followed.

**Budget for the harness (coordinator's reading, stricter than the worker's 1 KB):** a
hook may use at most **256 bytes** of stack including its callees and the DOS call, never
recurse, never call `printf`-class library routines. With far buffers nothing more is
needed, and 2,134 + 256 + interrupts stays under 2.6 KB of 4,454. Task 6 adds a stack
check to the replay build (the same pattern fill, lowest address written to the dump), so
every recorded session measures its own depth and the unreached paths get covered as the
sessions grow.

Rerun: scratch only (`probe_patch.py`, `sessions.py`, `static.py`); the method moves into
the replay build in task 6, not into the tree as is.

## 10. Phase 1 task 3: `replay.c` for Ultima VI (2026-11-16)

In the fork, commit `e0039a6` (local; the submodule is pinned to it). With
`-DRP_FAR_STORAGE` the harness keeps every variable and string in a far segment `RP_FAR`
(`runtime/replay/rpfar.asm`, class `'CODE'` so that TLINK places it before DGROUP), does
its file I/O through int 21h and has no near data: linked into GAME.EXE with a stub
caller, `_DATA` (3228h), `_BSS` (BBCCh) and `_BSSEND` (EDFAh) are unchanged and all 340
DGROUP publics keep their offsets. Stack per entry point: at most 236 bytes (static, every
path), 154 measured in emu2.

Bindings for the later tasks: `RP_RAND_SEED()`, `RP_NULL_SUM` 0DFAh with
`RP_NULL_SUM_TO`, `RP_STOP_KEY(r)`, `RP_SRAND`, `RP_WALL_READ`, the I/O and buffer-size
macros, `RP_MAGIC_TAG`; in `hookgame.h`, `HOOK_KEY_ARG` (the hook is `KEY(a)`) and
`HOOK_RECORDED` (`RECORDED(id, x)`, `rp_value`, MISC tag 10).

Underworld: its replay objects are identical but for COMENT records (review made
`rp_value` conditional on `HOOK_RECORDED` to get that); the UW1 and UW2 ports replay all
17 and 13 sessions identical to DOS. Not run: Underworld's own DOS-side golden check.

Open for tasks 4 to 7: `RP_KEYSTATE_LEN` must be at least 1 and Ultima VI keeps no key
state; `RP_FAR_SIZE` lives in both `replay.c` and `rpfar.asm` (MASM 3.00 has no `/D`);
in far mode the harness's variable names are macros and can capture a game variable of the
same name inside a binding's `RP_DUMP`; `rpfar.asm` is not yet in `tools/replay.py build`.

## 11. Phase 1 task 4: the hooks in GAME's C (2026-11-16, done by the coordinator)

`SRC/hookgame.h` now defines `HOOK_KEY_ARG`, `ORIG_KEY(a)`, `HOOK_RECORDED`, the ids of
the recorded values and `RECORDED16` (a recorded value cast back to int: `rp_value` gives
a long, which Turbo C would push whole to a function without a prototype).

| Where | What | Hook |
|---|---|---|
| `console.c` `CON_getch`, `main.c` the disk error prompt | `KBD_GetKey(a)` | `KEY(a)` |
| `main.c` `main` | `OSI_timeSeed()` | `RECORDED16(RV_SEED, ...)` |
| `main.c` `main` | `farcoreleft()` | `RECORDED(RV_FARCORE, ...)` |
| `disk.c` `DISK_init` | DOS version (AL, AH); per drive IOCTL 4408h carry and AX, 440Eh AL; int 11h AX; `OSI_getDrive()` twice | `RECORDED16(RV_..., ...)` at each use |

Nothing else in GAME's C reads from outside the program: the two int 10h calls in `main.c`
set the palette and read nothing back; every other read is in the assembly library
(task 5: the clock, the mouse, the delay calibration, the file reads' errors).

Exit tests: the gate passes (the hooks are the original tokens in the DOS build). The three
files and `replay.c` compile with `-DREPLAY` under Turbo C 2.0 and link into GAME.EXE with
`rpfar.asm`: `_DATA`, `_BSS`, `_BSSEND` and all 340 DGROUP publics unmoved. The port builds
with pass-through `rp_key` and `rp_value` (`port/sys/rphooks.c`, until Phase 2 links
`replay.c`) and replays a scripted session with all 225 checkpoints identical.

Not proven here: that the hooks record and replay correctly in DOS. No replay build runs
yet (it needs task 5's modules and task 6's `rpgame.h`; the link above used task 3's stub
bindings).

`tools/dosbuild.py` now stages `hookgame.h` with CRLF line ends: git stores this one file
with LF, and Turbo C 2.0 reads an LF-only header as one line.

## 12. Phase 1 task 5: the assembly library's outside reads (2026-11-25)

Done by one worker, reviewed; the gate and the map check rerun by the coordinator.

- `SRC/OSILIB/MOUSERP.ASM`, the replay build's MOUSE.ASM (its data segment the same byte for
  byte): the reset's AX and BX through `rp_value` (`RV_MOUSE_RESET`, `RV_MOUSE_NBTN`); each
  poll through `rp_mouse` then `rp_mbuttons`, the one int 33h made by `u6_mouse_poll`
  (`RP_MOUSE_READ`) into three words in the code segment. x is stored halved, as the game uses
  it, so the stream is in 320-pixel coordinates the port can give.
- `SRC/OSILIB/DELAYRP.ASM`: the int 1Ch handler counts in a code-segment word, so `D_305A`
  stays 0 in every dump. `OSI_delay` still waits in real time while replaying (follow-up: an
  `rp_mode` accessor in the fork would let it return at once).
- `RECORD_VAR(id, lv)` (hookgame.h), empty in the DOS build: `CpuSpeed` (`RV_CPUSPEED`) and
  int 12h's `D_2FC8` (`RV_MEMSIZE`, read nowhere but in the dumps) after `OSI_lowInit`.
  `D_2FC8` and `D_3088` (the rand state, for `RP_RAND_SEED`) made PUBLIC; the EXE is unchanged.
- `SRC/rpgame.h`, minimal: stop key F9 (143h; int 16h AH=0 never gives F11/F12), the mouse,
  key state and joystick lvalues outside DGROUP, `RP_SHUTDOWN` = `rp_u6_shutdown` (main.c,
  REPLAY only). `RP_CLOCK`, `RP_TICK_US`, `RP_PIT_DIVISOR` and the dump (DS:0..EDFAh plus the
  common sections) are placeholders marked TASK 6.

Exit tests: gate PASS (3 of 3). Replay link: `_DATA`, `_BSS`, `_BSSEND` and all 342 DGROUP
publics unmoved. Headless DOSBox-X, a staged character (an OBJLIST made by the port): recorded
`space up up left f9`, stopped at hook call 153198, RECORD.OUT 338 bytes with every VALUE
(CpuSpeed 672, memory 640, mouse FFFFh/3 buttons) and the MOUSE and BUTTONS streams. Replayed
from it: ran to the end, no desync; all 6 dumps equal to the recording's, VGA, PAL and RAND
included, except DS:0098, C0's `__STARTTIME` (the BIOS tick at start-up): task 6 masks it.
Port: builds, a scripted session 707 of 707 checkpoints identical. Stack (static, every path):
rp_mouse 212, rp_mbuttons 208, rp_value 168 bytes, within 256.

Other outside reads found, left for later: the int 24h handler's `D_2FCC`, write errors via
the OSI_FILE callbacks, LOW's calibration counter `D_2FCA` (ends at 1), the music driver's
timer service and the graphics driver (task 6).

Rerun: scratch only (`rpbuild.py`, `rprun.py`, `rpdecode.py`, `rpcmp.py`, `rpstack.py`); the
link becomes task 7's.

## 13. Phase 1 task 6: the dump (2026-11-26)

Done by one worker, reviewed; the gate, the map check and the replays rerun by the coordinator,
who changed the stack fill (below).

`SRC/rpgame.h` dumps, in order (bytes with VGA, mouse, AdLib under DOSBox-X):
- **Once the graphics driver has its memory (`RP_LEVEL`):** GSTA (DS:2C4A..2CCB, 130), OBJS (the
  five object arrays, 30720) and NPCS (the rest of OBJLIST's NPC tables and the party, 5105).
  Together they are a saved game's OBJLIST byte for byte.
- **Always:** DATA (DS:0..322D, 12846) and BSS (DS:322E..EDF9, 48076).
- **The far heap, once the graphics driver has its memory:**
  - SEGS: 30 segment words, for `[replay.segments]`.
  - GDRV (the mouse buffers and the graphics driver, 16888).
  - MDRV (the music driver, 1662).
  - FARB (the game's far blocks, 60098).
  - GRMM (the graphics driver's tile cache, 270544).
- **Always:** STAK (the stack check, 6), RAND, NULL, CNTS.
- **In every dump, since every Ultima VI dump is full:** PAL and SCRN, a linear
  A000:0000 frame of 64000 bytes. Chain-4 makes `dump_screen`'s four planes the same 64 KB four
  times, and `replay.py`'s scanout cannot draw them; `replay.py` needs a SCRN reader (task 7
  or Phase 2).

Masks, listed for `[[replay.mask]]` (task 7 writes them into `exhume.toml`), none zeroed:
- DATA 98h..9Bh, C0's `__STARTTIME`, which nothing reads.
- MDRV 0Ch..161h, U6ADLIB.DRV's variables, stepped by its 60 Hz int 8 handler. The offsets are
  AdLib's; another music driver needs its own.
- STAK 4..5, the lowest address written, which is a measurement.

`RP_CLOCK` is the BIOS tick, but no hook reads it: Ultima VI never calls GAME_TIME, so there
are no periodic dumps. Dumps come at each key, each button change and the end; `UWRPHOOK`
bisects. Possible later: a CHECKPOINT at each turn, for long AI-only stretches.

Stack check: `SRC/OSILIB/STACKRP.ASM` (replay build only, code-segment storage) fills from
`__brklvl` (EE9Ah) to 40 bytes below the first hook's SP (FFB0h), through a new optional binding
`RP_START()` in the fork (`46c7857` on `u6-tc20`, +5 lines, Underworld unaffected). The worker
filled with A5h. The coordinator changed it to **zeros**: any pattern changes what the game's
uninitialised locals read, so the replay build would no longer behave as GAME.EXE (DOSBox-X's
memory is zero there, and the port's stack junk is 0). The lowest non-zero byte is a floor, and
it reads the same here: lowest FC22h, 990 bytes below 10000h, matching section 9.

Exit tests:
- Gate PASS.
- `_DATA`, `_BSS`, `_BSSEND` and all 342 DGROUP publics unmoved.
- Worker's session `rec7` (moves, a look, a failed talk, turns passing; 27 dumps of 512 KB):
  two replays identical apart from the masks, and so are the recording and its replay
  (rerun by the coordinator).
- After the zero fill, the coordinator's session `z1` (moves, a look, a conversation with
  name, job and bye, turns passing; 30 dumps): the same result.
- The port builds; a 60 s scripted session matched at 927 of 927 checkpoints (worker).

Open:
- One live recording by the worker hung after "bye" (A5h fill, killed at 400 s, no
  recording). The same keys completed on both fills afterwards; the cause is unknown.
- Only AdLib and VGA were tried.

Rerun: scratch (`task6/RERUN.txt`: rpbuild, rprun, rpcmp, rpdecode, rpstack, shots); the build
becomes task 7's.

## 14. Phase 1 task 7: the replay build and its runs as tools (2026-11-26)

Done by one worker, reviewed; the gate, a clean build and `rp.py check` rerun by the coordinator.

- `exhume.toml` (new): `[replay]` with the three masks and their reasons, `[replay.segments]`
  (DGROUP paragraph 329Bh; untested until Phase 2), stop key F9, the DOSBox-X conf.
- `tools/rpbuild.py`: GAME.EXE checked against the gate's sha1, then RGAME.EXE. It fails if any
  DGROUP public moved, and is cached by a sha1 over its inputs. Exhume's `replay.py build` runs it
  through a new `[replay] build` hook.
- `tools/rp.py`: `build`, `record` (ours: DOSBox-X AUTOTYPE, F9 appended; `replay.py record`
  needs js-dos), `replay`, `compare`, `show`, `log` (Exhume's `replay.py`), and `check` (record,
  replay twice, compare both pairs, PNGs). `make replay`, `make replay-check`. `docs/replay.md`.
- `tests/replay/`:
  - `stage/SAVEGAME/OBJLIST`: the character, made by the port's `--new-avatar`, reproducible
    byte for byte. Mostly the installed game's own starting OBJLIST.
  - `sessions.toml`: `smoke`, a minute of the opening fight.
  - `dosbox-x.conf`: replaydos's default. Recording and replay use the same conf, switches and
    batch, since a different start-up moved the load segment by a paragraph.
- Fork `dd92217` on `u6-tc20`: `[replay] build`, `stop_key`, and the SCRN frame in
  `scanout`/`compare`/`show`. All of it is inactive unless configured. On synthetic Underworld
  dumps the output is unchanged (Exhume has no tests of `replay.py`).

Exit tests:
- Gate PASS.
- A clean `rp.py build`: DGROUP unmoved, RGAME.EXE sha1 `def703cb...`, the same as task 6's
  scratch build.
- `rp.py check`: 27 dumps; recording = replay 1 = replay 2 apart from the masks, read from
  `exhume.toml`. With the masks removed, DATA 98..99 differs.
- The last PNG is the throne-room fight with the party panel.
- Port: 1108 of 1108 checkpoints identical (worker).

Open, for task 8:
- **Recordings that hang at a conversation:**
  - once in task 6 (after "bye");
  - twice here, both at the same byte of STATE.OUT, while the conf began with a comment block,
    and never without it.

  The cause is unknown, and it could be the game, the harness (a dump of half a megabyte at each
  key, the stack fill) or DOSBox-X. Task 8 should find it before relying on long sessions.
- A WSL (snap DOSBox-X) replay of a Windows recording took the same path but differed in a few
  DATA/NULL/STAK/BSS bytes, so compare only within one DOSBox-X install.
- Golden PNGs, checking the saved games, js-dos recording (`stop_key` is passed, untested).

## 15. Phase 1 task 8: scripted character creation and the first real session (2026-11-26)

Done by one worker and reviewed. The coordinator reran the gate, a fresh `rp.py chargen --force`, `rp.py check --session first` and `--rec tests/replay/first.rec`.

- **The character comes from a script, as Underworld Exhumed's `_chargen` does.**
  - `[_chargen]` in `tests/replay/sessions.toml` drives the real U.EXE in DOSBox-X: Esc, C, "Avatar", male, the first portrait, then A to all seven questions.
  - The result is Avatar, strength 17, dexterity 20, intelligence 22. OBJLIST is 7539 bytes, sha1 `01572292...`. `rp.py` checks it against `objlist_sha1` and fails loudly on a mismatch.
  - It is cached in `build/dos/replay/stage` by a sha1 of its inputs.
  - While U.EXE runs, GAME.EXE is a 63-byte stub that exits.
  - The committed `tests/replay/stage/OBJLIST` is gone.
- **Why U.EXE needs help:** `srand(time(0))` shuffles the gypsy's questions, and `DrawPattern` calls rand() every frame after the first answer. No fixed answers give the same character under every order.
- **`tools/keyfeed.py` builds KEYFEED.COM**, a small TSR that the Python assembles itself; `--list` prints the listing. It types a key script counted in the program's own keyboard polls, not in time, and it gives the program a fixed date and time.
  - Mode `dos`: int 21h 0Bh, 07h and 08h, for U.EXE.
  - Mode `bios`: int 16h 01h/11h and 00h/10h, for GAME. This mode also fixes OSI_timeSeed's seed and can type Ctrl+S.
  - It is loaded with `LH`, so conventional memory matches a replay. A replay doesn't load it: the keys come from RECORD.OUT.
  - Two new masks, NULL 8B..8E and B7..BA, cover the int 16h and int 21h vectors, which point into KEYFEED only while recording. The game neither sets nor reads them.
- **The conversation hang was DOSBox-X's AUTOTYPE**, not the game or the harness:
  - 3 of 37 AUTOTYPE recordings hung.
  - In one, the game kept polling (2,484 empty KEY polls) and the last keys never reached the BIOS buffer.
  - In another, DOSBox-X itself froze.
  - The comment-block link was coincidence. It did expose a bug: `str.replace('[cpu]')` wrote cycles into a comment. This is now a section-aware `conf_set`.
  - With KEYFEED, the formerly hanging setup recorded 5 of 5 runs, all with the same RECORD.OUT.
- **The first real session is `[first]`, recorded as `tests/replay/first.rec`.** It covers:
  - a talk with Dupre (name, job, join, bye);
  - the inventory (F1, F10);
  - a look at Lord British;
  - a fight with six attacks north;
  - six steps out of the throne room and ten passes;
  - a save with Ctrl+S, Y.

  It has 70 dumps. A fresh recording is byte-identical to `first.rec` (`679203c9...`).
- **`rp.py check` now compares the saved games** too (`[replay] saves = ["SAVEGAME"]`).
- **Recorded in DOSBox-X, not js-dos:** js-dos loads GAME 680h paragraphs lower, so the recorded `farcoreleft` differs and a DOSBox-X replay stalls.

Exit tests:
- Gate passes, 3 of 3.
- chargen: the same OBJLIST sha1 on every run, including runs at fixed cycles, at max cycles and under WSL's DOSBox-X. A different clock gives a different character, which shows the clock really is the seed.
- `rp.py check --session first`: recording = replay 1 = replay 2 apart from the masks, and the saved games are identical. `--rec first.rec` passes. `make replay-check` passes in WSL.
- Port: builds; a 60 s scripted session replays 1510 of 1510 checkpoints identical (worker).

This closes Phase 1. Open:
- Why AUTOTYPE fails inside DOSBox-X: untraced, and avoided by using KEYFEED.
- js-dos recordings replayed in DOSBox-X: the load segment differs.
- The session has no outdoor walking and no day turning to night.
- `[replay.segments]` is untested until Phase 2.
- Golden PNGs.
- Only OBJLIST is checked. Sessions still take the other SAVEGAME files (the OBJBLKs) from the user's install unchecked.

## 16. js-dos and DOSBox-X agree; Exhume updated (2026-11-26, by the coordinator)

- **Exhume:** merged upstream `abedegno/Exhume` master (81 commits, clean) into `u6-tc20` (`098996a`). Upstream didn't touch the replay code. RGAME.EXE is unchanged (`def703cb...`), and `first.rec` replayed identically on it.
  - New fork commits: `fb89053`, `[replay] dos_env` / replaydos `--dos-set`; `c4244a5`, its docs.
- **Why js-dos and DOSBox-X differed:** DOSBox-X loaded GAME at PSP 812h, js-dos at 192h, 680h paragraphs apart. GAME records `farcoreleft`, so a js-dos recording promised a DOSBox-X replay 26 KB it didn't have, and the replay stalled. Two causes:
  - DOSBox-X's `minimum mcb free`: when it is 0, DOSBox-X uses 700h and pads with a DOS-owned block up to that segment.
  - DOSBox-X's environment: a longer PATH, a PROMPT, and 1024 bytes of slack per program environment block where mainline DOSBox gives 83. Turbo C's start-up sizes the near heap by the environment.
- **Fix, all in our configuration:**
  - `tests/replay/dosbox-x.conf` `[dos]`: `kernel allocation in umb=true`, `minimum mcb free=BE`, `additional environment block size on exec=83`, `drive z expand path=false`.
  - exhume.toml `[replay] dos_env`: PROMPT removed, then PATH, COMSPEC and BLASTER set. The same lines go to both DOSes and to `rp.py record`'s batch.
  - A probe COM measured it: PSP 192h, memory top 9FFFh, environment segment 188h, and the same environment bytes in both DOSes.
- **Proof:**
  - A js-dos recording replays in DOSBox-X, and the new `first.rec` (recorded in DOSBox-X, sha1 `900d1655...`) replays in js-dos.
  - In both directions, every checkpoint is identical apart from addresses in each emulator's BIOS (DATA 75..81h and 305Eh, the NULL vector table). The saved games are identical.
  - WSL: `make replay-check` passes. `first.rec` replayed under WSL's snap DOSBox-X now differs from the Windows run only in BIOS addresses (DATA 78h, the vector table). This closes task 7's open item, which was the environment.
  - DOSBox-X 2026.10.01 gives the same layout and differs from 2022.12.26 only in BIOS addresses. The bundled 2022.12.26 stays.
  - `first.rec` was re-recorded because the layout changed. Two recordings gave the same sha1. `rp.py check --session first`: recording = replay 1 = replay 2 apart from the masks, and the saves are identical. Character creation is unchanged (`01572292...`).
- **Open:**
  - A mask scope for two different DOSes (Exhume has `always` and `cross`).
  - Phase 2's `cross` masks will need the C runtime's saved vectors (DATA 74..83h).

## 17. Phase 2 task 1: the port links replay.c and replays DOS's recording (2026-11-26, by the coordinator)

Phase 2 split into four tasks:
1. **Link replay.c.** The port makes the same hook calls and writes the byte-portable sections.
2. **DATA, BSS and STAK in DOS's layout.** Each global is written in DOS's bytes, with pointers as segment:offset. The graphics driver's state too.
3. **Fix every divergence:** each one either fixed or masked with its reason. Linux and Windows, gcc and clang.
4. **Retire the port's own recorder** (`port/replay`, `digest.c`) in favour of Exhume's format, with a black box.

Task 1:
- **The bindings.** `SRC/rpgame.h` includes `port/include/rpport.h` when the compiler isn't Turbo C. `port/game/rpport.c` is built with the game's sources: the SEGS words through the real structs, the far heap's bounds, the file paths, and stand-ins for the runtime pieces replay.c expects. `port/include/io.h` covers Linux. `replay.c` is compiled in place from the submodule, with RP_FAR_STORAGE as in DOS. `port/sys/rphooks.c` is gone.
- **Changes to the port's own code:**
  - `mouse.c`: the poll goes through `rp_mouse` then `rp_mbuttons`, with `u6_mouse_poll` as RP_MOUSE_READ. MOUSE_INIT records the reset's AX and BX (RECORDED 12 and 13) and returns BX, as MOUSERP.ASM does.
  - `rand.c`: `u6_rand_seed`.
  - `portmain.c`: `--dos-replay FILE`. Exhume's `rp_request` is off otherwise.
  - The port's own recorder still runs on top: a `--keys` session still replays 761 of 761 checkpoints identical.
  - The OSI library tests pass: 257 unit checks, and the Unicorn diffs (RAND, SEED, LZW, KBD, SOUND and MOUSE against GAME.EXE's own routines). Running them in WSL needed `U6_DOS_EXE=build/dos/game/GAME.EXE` and unicorn in a venv (`~/u6venv`). `port/tests/osi_stubs.c` has the three hooks, passed through. Its `rp_checkpoint` was renamed `rp_digest_checkpoint`, since replay.c has its own.
- **`[replay.segments]`, first real use.** The tile cache keeps GrMemory's page segments, 305Eh paragraphs above FarHeap in both builds. `exe_range` now runs up to 9E00h, so the `heap_block` rule covers the far heap; after that, GRMM is identical.
- **Tools.** `rp.py port REC OUT` (on Windows, the port's WSL build at `~/u6port-replay`), `rp.py port-build`, and `rp.py port-check OUT [--rec]`. port-check runs a DOS replay and a port replay, compares them across builds, then compares the saved games: every file DOS's run changed must be in the port's home, identical.
- **Result on `first.rec`:**
  - All 70 checkpoint headers are identical: the same path and the same hook calls.
  - From checkpoint 1 on, GSTA, OBJS, NPCS, FARB, MDRV, GRMM, RAND, CNTS, PAL and SCRN are identical at every checkpoint.
  - Saves: the 5 files the session wrote are identical.
  - RGAME.EXE is unchanged (`def703cb...`).
- **Left for tasks 2 and 3:**
  - DATA, BSS and STAK (not written by the port).
  - GDRV: the graphics driver's image. DOS's driver keeps `struct tGr` and its tables there; the port's C driver keeps them in host memory, and GrInfo's segment is a foreign F001h.
  - Checkpoint 0's PAL and SCRN, before mode 13h: DOSBox-X's text mode against the port's black.
  - The port's replay waits in real time (about 3 minutes).
  - The NULL section is a stand-in (DS:0's bytes, a zero vector table).

## 18. Phase 2 task 2: DGROUP and the graphics driver in DOS's layout (2026-11-26, by the coordinator)

The port now writes every section DOS writes, and `first.rec` is identical at 69 of its 70 checkpoints, with the masks applied. The saved games are identical.

- **DATA and BSS.** `port/tools/gen_dgroup.py` (`python tools/rp.py port-dgroup` reruns it from a port build's `nm`) generates `port/game/dgroup_port.c`: 905 DOS byte ranges of DGROUP, with where the port keeps each. The sources:
  - BSS.ASM's 166 objects from `port/sys/bss.c`. Their DOS offsets are counted in order and agree with every offset comment in u6.h.
  - The DATA publics of RGAME.MAP, matched to port symbols by name.
  - The statics the game's sources give a DOS offset for, reached through generated wrappers that compile the source and add a table (`port/game/tu_NAME.c`, 10 sources), and the port's own D_ statics (`dgs_NAME.inc`: mouse.c, osi.c, sound.c).
  - The gamestat block, RAND.ASM's seed, and the one writable literal (the NPC name buffer at DS:092E, via `WRITABLE_STR`, which now registers its copy).

  `u6_rp_dgroup` writes the image, with each pointer in DOS's form:
  - a far pointer as the arena's normalised segment:offset, or DGROUP (0):DOS offset;
  - a near pointer as its DOS offset;
  - a code pointer as RGAME.MAP's segment:offset, counted from DGROUP's paragraph. 360 functions, plus `DISK_NullCallback` (static, DISK_TEXT:0003);
  - GrDriver and MUS_Driver as (the last entry called, the driver's segment). The port's `GR_*`/`MUS_DRV` macros and mouse.c now keep that entry number, as DOS's offset word does.
- **GDRV:** the graphics driver's segment as DOS's keeps it (`mcga.c`'s `drv[]` at DOS offsets, and the tGr block serialised in DOS's layout at CS:0052; GrInfo points there). Before, it differed in about 5,200 bytes; now it differs only in the driver's mouse temporaries (masked).
- **Fixes in the port:**
  - `GrMemorySeg` cast a host pointer to a long. The new `FARPTR32(p)` (`u6port.h`) is the original `(int32)(p)` under Turbo C. The gate passes, and RGAME.EXE is unchanged (`def703cb`).
  - `OSI_playNoise`'s sum (DOS's D_3092) is kept as DOS keeps it.
  - `struct u6_dgs` has a fixed size, because the game's sources pack structs and the port's files don't.
- **Masks, `scope = "cross"`.** 58 in all:
  - Generated by `gen_dgroup.py --masks`: Turbo C's run-time data and library data and BSS, the modules' string literals, INFLATE.ASM's work variables, OSILIB data the port keeps another way, and the near string-pointer tables.
  - By hand: STAK's measure, OSI_ErrorFile, ChunksFile/MapFile (DOS handles, shifted by the replay build's own files), VarStr (but its 'N' slot), and the DOS driver's mouse temporaries.
  - Masks that cover bytes DOS changes during the session: OSI_ErrorFile, VarStr, the driver's temporaries, INFLATE's variables, the near heap's pointers. Every other masked byte is constant.
- **Checks:**
  - Gate 3 of 3.
  - `port-check` on first.rec: 69 of 70 identical; saves: the 5 changed files identical.
  - The port's own `--keys` record/replay: 840 of 840.
  - OSI tests: 257 unit checks, plus the RAND, SEED, LZW, KBD, SOUND and MOUSE Unicorn diffs.
  - `mcga_diff.py`: PASS.
- **Open, for task 3:**
  - Checkpoint 0, before the level exists. DOS writes no SEGS there, so its segment words can't be paired. Writing SEGS in every dump would change the replay build's dump code. Also, its screen and palette are DOSBox-X's text mode.
  - The Windows build, clang, and other sessions.
  - Near pointers to string literals are masked rather than compared.
  - The port's replay still waits in real time.
  - A flaky `cpSync` failure of one DOS replay, seen once while a WSL build was running in the same tree.

## 19. Phase 2 task 3: every checkpoint, four builds (2026-11-26, by the coordinator)

`first.rec` and a second session, `smoke`, are identical to DOS at every checkpoint, in every section apart from the masks. The saved games are identical. This holds in four builds: Linux gcc 11 and clang 14 (WSL), and Windows MinGW gcc 16 and clang 23 (MSYS2). The Windows gcc run is also identical to the Linux gcc run under `--same-build`, which applies only the `always` masks and none of the cross ones.

- **Checkpoint 0**, before the level exists:
  - **Segment words.** DOS writes SEGS only from the level on. Exhume's `replay.py compare` now pairs a dump without SEGS against each run's first SEGS, using only the load segment's rule and `heap_block`'s, since neither moves during a run. Exhume docs/config.md says so.
  - **Mouse.onMove** is the DOS driver's default handler, `_MOUSE_0055`. The port's handler (a static `mouse_dummy` before) now carries that name. `gen_dgroup.py` also matches the assembly library's underscored names (21 more functions, 381 in all).
  - **PAL and SCRN.** DOS's VGA is still in text mode 3 at this point. While the game hasn't put the VGA in mode 13h (`mcga_mode13`: GR_INIT to GR_SHUTDOWN), the port writes what text mode holds: the VGA BIOS's mode-3 DAC (the EGA's 64 colours) and FFh for A000:0000, which nothing maps in text mode. This is a model of the machine, like NULL's stand-in, and the port draws nothing differently.
- **Speed.** A `--dos-replay` run took about 3 minutes and now takes 1 second. `timer.c` runs a virtual clock there, which OSI_delay's waits advance. Nothing the game decides on reads the tick count (DELAYRP.ASM keeps it outside DGROUP). The run reads no live input, and the busy delay loops return at once, as in the port's own replays.
- **The flaky `cpSync`.** The cause: `chargen()` returned the cached stage, then rpbuild rebuilt the replay build and emptied `build/dos/replay`, the stage folder included. The stage now lives in `build/dos/stage`.
- **Tools.** `rp.py port-build/port-check --variant V | --all` (linux-gcc, linux-clang, win-gcc, win-clang; `U6_MSYS2`). port-check builds every variant first, runs DOS once, then compares each build with DOS and prints a summary.
- **Checks:**
  - port-check `--all` on first.rec and smoke (recorded this task, `tests/replay/smoke.rec`): all identical, saves identical. Once the builds are done, the whole check takes about 1.5 minutes, most of it the DOS replay.
  - The port's own `--record`/`--replay`: 107 of 107.
  - OSI tests: 257 unit checks, and the RAND, SEED, LZW, KBD, SOUND and MOUSE Unicorn diffs.
  - `mcga_diff.py`: PASS.
  - No SRC file changed. RGAME.EXE is unchanged (`def703cb`).
- **Masks.** No new ones: 58 cross masks, all from task 2.
- **Left for task 4 and Phase 3:**
  - Retire the port's own recorder.
  - Near pointers to string literals are still masked, not compared.
  - More sessions.
- **Decided (the user, 2026-11-26): test set-ups go through saved games, not runtime cheats.** A Phase 3 tool will edit the staged saved game: positions (teleport), attributes and levels, inventory, quest flags, and the global state block. DOS and the port then start from the same file, so a session stays proved against DOS. Exhume's runtime cheats are port-only, and DOS can't replay them.

## 20. Phase 2 task 4: the port's own recorder retired (2026-11-26, by the coordinator)

The port records and replays through Exhume's `replay.c` only. `port/replay/` (RECORD.U6R: streams of ticks, keys, shift, mouse, seed and date below the game, and a digest every 4096 reads) and `port/game/digest.c` are gone.

- **What replaced each stream.** The game's own hooks already held every outside value it decides on: KEY, the mouse poll, and the RECORDED values (seed, far memory, DOS version, drives, CPU speed, mouse reset). The port's extra streams were all below them or decided nothing:
  - Keys, shift, mouse: under KEY, MOUSE and MBUTTONS, which record the results.
  - Seed: under `RECORDED16(RV_SEED, ...)`.
  - Date and time: only OSI_timeSeed reads them.
  - Ticks: nothing the game decides on reads them (task 3). Every replay now runs on task 3's virtual clock, paced to `--speed` (`osi_set_replay_speed`). `session_pace_tick` and the tick stream went with it.
- **The modes** (`port/sys/session.c`, `u6_rp_set_*` in `port/game/rpport.c`):
  - The black box: every player's session, `recordings/YYYYMMDD-HHMMSS/RECORD.OUT` and `stage/`, through replay.c's `rp_blackbox`. No dumps, no stop key. It is closed at exit, when the window closes, and at a fault: session.c catches SIGSEGV, SIGFPE, SIGILL and SIGABRT, calls `rp_blackbox_close(1)`, then re-raises.
  - `--record DIR`: with the dumps; F9 ends it.
  - `--replay REC`: shown to the player. It runs in a temporary home with no dumps (`rp_blackbox` also turns off STATE.OUT) and exits at its end.
  - `--replay REC --out OUT`: the replay runs in OUT, with the dumps.
  - `--dos-replay FILE`: unchanged, and it also takes the port's RECORD.OUT.
- **Exhume:** a port-only `rp_take_over()` lets the settings screen's "Take over" end a replay between two hook calls, after which the hooks read the world. It is not in the DOS build (`#ifndef __TURBOC__`). `--continue` is gone: replay.c ends a replay inside the hook that runs out of input, where the game can't go on.
- **`--keys`** now gives each key once the game has taken the one before. On real time, keys typed while the game was busy were lost to `KBD_GetKey(1)`, which keeps only the last one in the buffer, and the save in the test session happened in one run out of three.
- **Tools:** `rp.py port-self OUT [--all]`, the port against itself:
  - a `--record` session (a scripted walk, a look, passes, ctrl+s Y), replayed with `--out`, with both runs' dumps compared (`--same-build`) and their saved games;
  - the same keys as a black box session (ended by `--run-ms`), whose replay must leave the same saved game.
  
  It replaces the old `--replay` digest check, the "107 of 107".
- **Checks:**
  - port-self `--all`: identical in all four builds (14 checkpoints; OBJLIST and four OBJBLK files after both replays).
  - port-check `--all` on first.rec and smoke: still identical to DOS, saves identical, all four builds.
  - OSI tests: 255 unit checks (the replay test now checks the virtual clock and take-over), and the RAND, SEED, LZW, KBD, SOUND and MOUSE Unicorn diffs.
  - Gate 3 of 3. No SRC file changed. RGAME.EXE is unchanged (`def703cb`).
- **Next:** the save editor for Phase 3's set-ups (`tools/u6save.py`).

## 21. The save editor: test set-ups through the saved game (2026-11-26, by the coordinator)

The user's decision (section 19) carried out. `tools/u6save.py` edits a saved game in the game's own formats, and a session's `setup` in tests/replay/sessions.toml lists its commands. `rp.py`'s `session_stage` applies them to the character's stage, in `build/dos/stage-NAME`. The cache key is the commands, the character's OBJLIST and u6save.py.

- **Commands:**
  - `show`;
  - `set` a state-block field, with gamestat.c's names parsed from the source;
  - `npc WHO FIELD VALUE`, an NPC's attributes, level, flags and name;
  - `teleport X Y Z`: the party and their belongings, moved to the new area's file;
  - `give WHO TYPE [QUANTITY [QUALITY [FRAME]]]`.
- **The formats** come from the game's code:
  - OBJLIST from GAME_Load and GAME_Save;
  - the area files from OBJ_UnloadArea and `__ObjSerialize`: held items in the file of the area where the owner stands, contained items as container indexes, order by OBJ_CompareOrder.
  
  All 69 area files of the game read and write back byte for byte.
- **Plumbing:** record, check, replay, port and port-check take `--session NAME` and run from that stage. DOS's runs copy it over the game. The port's runs put it in their home folder, and port_saves counts it as the base.
- **Test session `[setup]`:**
  - setup: the party teleported to the first moongate's place, (3A7h, 106h, 0), in area file OBJBLKHC instead of the castle's OBJBLKCC;
  - 100 gold coins for the Avatar, the Avatar's strength 30, Dupre's level 6, karma 99, noon;
  - then steps, the inventory, the party, passes and a save.
- **Result in DOS** (`rp.py check build/setup --session setup`):
  - recording = replay 1 = replay 2 apart from the masks, and the saved games are identical;
  - the screens show the party on the moongate's stone circle;
  - the saved game DOS wrote at the end keeps every edit: strength 30, level 6, karma 99, the 100 gold coins, the party beside the moongate, the clock moved on to 12:05.
  
  The recording is `tests/replay/setup.rec` (sha1 781b5582...; 98f395d1... since the drake's fix, recorded again: Phase 3's drake session).
- **Result in the port** (`rp.py port-check OUT --rec tests/replay/setup.rec --session setup --all`): all 16 checkpoints identical to DOS in all four builds. The saved games are identical too: the 4 files DOS changed, and the 6 in the port's home.

## 23. Where things stand, and what comes next (2026-11-26)

### What is proved

- **The port is DOS's program, session by session.** 18 sessions, each recorded in the replay DOS
  build (recording = replay 1 = replay 2) and identical to DOS in all four port builds (Linux and
  Windows, gcc and clang), every checkpoint and the saved games:
  - first, smoke: ordinary play;
  - setup: a session from an edited saved game;
  - shrine: melee, ranged, spells, the party's death and resurrection;
  - drake: a halberd and a cannon at a jailed drake;
  - endgame, potions: the Codex vanishing, by moonstones and by eight potions;
  - fountain, shop: two port hazards;
  - ladders, dungeon: map transitions;
  - balloon, balloon5: the balloon, and on level 5 over the mountains to the shrine area;
  - ships: ships and skiffs;
  - keg: the powder keg's stale object number;
  - spells1, spells2, spells3: every one of the 80 player spells.
- **The port's own recorder is Exhume's** (Phase 2 task 4). The black box records every player
  session, and `rp.py port-self` proves the port against itself.
- **Test set-ups go through the saved game** (tools/u6save.py, section 21):
  - teleport, set, npc, give/put/equip/unequip, place;
  - map, the terrain around a place.
  
  All 69 area files read and write back byte for byte.

### What was learned about the original (docs/CODE_REVIEW.md, 146 findings; FINDINGS.md)

- **Confirmed in DOS:**
  - CR-001: the vortex cube takes any item dropped onto it and checks only frames 0..7, so eight
    potions end the game;
  - CR-002: the inventory hangs for good after readying from a small bag and scrolling;
  - CR-004: the powder keg's fuse deletes whatever takes its object slot;
  - CR-008: an empty wish at a fountain matches, through pointers read past the wish table.
- **Moonstones:** burying is reversible (Get), and a buried stone can be found by its moongate,
  by the moongates of its phase and by Gate Travel.
- **Uninitialised or out-of-bounds reads** that DOS and the port must agree on:
  - gfx.c's never-drawn missile position (STACK_JUNK_FILL);
  - the talk interpreter's lstack[-1], which is the caller's saved registers;
  - the fountain's table.
- **Argument order:** GAME is -p (left to right), U.EXE cdecl (right to left). None are left in
  GAME after the three fixed.
- **The user's GOG GAME.EXE** is the same 4.5 program, EXEPACKed.

### What was fixed in the port

- **Hazards:** CR-020 (AddObj's return), CR-008, CR-009, the argument order in three places, and
  STACK_JUNK_FILL in gfx.c. Each fix keeps the original text under Turbo C (gate 3 of 3).
- **END.EXE after a game:** it had no free memory (dosmem_reset).
- **The tools:**
  - DGROUP's paragraph is read from RGAME.MAP;
  - `rp.py port-check` checks exhume.toml's copy of it;
  - `--keys` waits for the game to take each key.

### Next steps

1. **The rest of the review's claims:**
   - ~~CR-005, CR-003~~: done, with every other critical one (section 24).
   - The high and medium findings, in order. Each is confirmed or refuted by a session where it
     can be reached, and the result noted in CODE_REVIEW.md's table.
2. **Phase 3's remaining sessions:**
   - walking and day/night, a long conversation, camping;
   - save and reload (ctrl+r);
   - a quest flag, set by the editor and then played through.
3. **Phase 3's test gate:**
   - goldens from DOS for every session, committed with the recordings;
   - `make test` (gate, port build, every session against its golden), a pre-push hook;
   - CI on Linux and Windows from an encrypted bundle.
   
   The port checks take about 1.5 minutes per session for all four builds. Run them one at a
   time: the machine runs low on memory, and Claude Code has stopped longer background runs.
4. **Phase 4: U.EXE and END.EXE under the same proof:**
   - a replay build for each and sessions (the gypsy, the introduction, the ending);
   - fix SRC_U/intro.c's four cdecl argument-order cases.
   
   END.EXE runs after the endgame session in the port now, but nothing proves it yet.
5. **Phase 7, enhancements:**
   - ~~the fixes of the critical findings~~: done (section 24, port/sys/fixes.c, `--fixes`);
   - the moonstone burial guard, as an enhancement (not a fix: the original does what it says),
     off by default (the user: no exception), beside the fixes; then the high findings'
     fixes as each is confirmed.
6. **Keep in mind:**
   - any change to the replay build's code size changes farcoreleft(), so every recording is
     made again (`rp.py check OUT --session NAME`, copy rec/RECORD.OUT to tests/replay/);
   - exhume.toml's first_block_para must follow RGAME.MAP's DGROUP (port-check checks it).

## 24. The critical findings nailed down, and the port's fixes (2026-11-26)

### Every critical finding, tested in DOS

Each one has a session; CODE_REVIEW.md section 5.0 has the table. The new ones:

- **cr003, CR-003:**
  - recorded in the port and replayed in DOS (below), since only the mouse sets a map target
    in cast mode;
  - Heal cast by clicking Tiberius decremented `SpellPageCounts[0x1A]`, which is DS:EBD8,
    `FormationObjs[3]`.
- **cr005, CR-005:** Tiberius raised a creature's corpse (slot E5h) carried ahead of a dead
  NPC's, and took the gold.
- **cr006 and cr006b, CR-006:**
  - both out-of-bounds cells were hit: `[0][-1]` and `[10][11]`, which is `CellObjNext[0]`;
  - the effect is milder than predicted: the beam's node ends up on the view's free list.
- **cr007, CR-007:**
  - 2,000 stones added to an area, then a save;
  - the area kept 2,771 of 2,864 objects, and the two areas loaded after it (Britain's 944, and
    74) were saved empty.
- **cr010, CR-010:**
  - reachable: the ocean runs from x=4 to x=1012, so two ships can lie across the seam;
  - the chain went out of order at the stern's move into row 500;
  - it needed the cache rebuilt after the far area loaded, which CR-019 delays until AreaY
    changes.

### Tools added

- **The port's `--keys` script clicks the mouse:** `MS:click:X,Y`, `MS:rclick:X,Y` and
  `MS:mouse:X,Y`, with a mouse even under `--hidden`.
  - A click holds for 30 of the game's looks at the mouse. At 80 ms, it came and went between
    two looks of the command loop.
- **`--farcore N`:** the port reports DOS's farcoreleft (0x5BBB8), so a port recording replays
  in DOS.
  - The first try hung DOS: it was given the port's 0x8FFF8, and farmalloc failed.
- **`rp.py port-record OUT SESSION [--fixes]`:** records a session in the port, from a session's
  `port_keys` or its `keys`.
- **`rp.py port-check --fixes LIST`:** replays a DOS recording in the port with the fixes on.
- **u6save:** `fill TYPE X0 Y0 X1 Y1 Z` and `count X Y Z`.

### The fixes

- **Turning them on:** `--fixes all`, or `fixes=all` in u6port.cfg; off by default
  (docs/port/FIXES.md).
- **Code:**
  - port/sys/fixes.c is the registry; replay.c's `enhance_on`/`enhance_names` are its own, so a
    session with fixes is recorded in format 5 and replays with them;
  - nine sites in SRC under `#ifndef __TURBOC__` with `U6_FIX(...)`: CR-001 to 008 and CR-010.
    CR-009 needs none; the port handles it always.
- **The original builds:** gate 3 of 3, and the replay DOS build has the same sha1 (b2975855),
  so every recording is still valid.
- **Fixes off:** all 25 sessions identical to DOS in all four builds.
- **Fixes on:** the 15 sessions that reach no fault are identical to DOS. The bug sessions
  differ exactly where their fault is (FIXES.md's table).
- **What a fix can't change:** a healer still takes the gold for a creature's corpse alone,
  because the script takes it before the resurrect opcode runs; the fix only stops the wrong
  body being raised.
