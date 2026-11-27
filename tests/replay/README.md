# Replay sessions

What the replay DOS build's runs read (docs/replay.md):

- `sessions.toml`: key scripts for `tools/rp.py record` and `check`, and `[_chargen]`, the
  character creation every run starts from (below).
- `first.rec`: the first real session's recording (`[first]`; Exhume's layout, `NAME.rec` in
  `[replay] sessions`). `python tools/rp.py check OUT --rec tests/replay/first.rec` replays it
  twice and compares.
- `smoke.rec`: `[smoke]`'s recording (sha1 38457496...). `python tools/rp.py port-check OUT --rec
  tests/replay/smoke.rec --all` checks it against the port, like first.rec.
- `setup.rec`: `[setup]`'s recording (sha1 bc9cc110...), from its edited saved game; replay it
  with `--session setup`. Likewise `shrine.rec` (`[shrine]`, 484d23bc...), `drake.rec`
  (`[drake]`, 3e4fb65e...) and `endgame.rec` (`[endgame]`, b7a8df86...). first.rec is
  e01afca1....
- The critical findings of docs/CODE_REVIEW.md each have a session: potions (CR-001), invhang
  (CR-002, not a test: it hangs), cr003, keg (CR-004), cr005, cr006 and cr006b, cr007, fountain
  (CR-008), shop (CR-009), cr010. cr003.rec was recorded in the port (`python tools/rp.py
  port-record OUT cr003`: its `port_keys` click the map, which DOS's key feeder cannot) and
  replayed in DOS; the others in DOS. `rp.py port-check ... --fixes all` replays them in the
  port with its fixes on (docs/port/FIXES.md): each then differs from DOS where its fault is,
  and every other session stays identical.
- Videos: `python tools/rp.py port-check OUT --rec tests/replay/NAME.rec --session NAME --video
  build/videos` (or `rp.py port ... --video FILE`) writes the port's replay of a session as an
  .mp4. It is DOS's run, since the port's replay is identical to DOS's. The videos stay out of
  git (`build*/`).
- All of them were made with the replay build as it is: a change to that build's code size
  changes the far memory GAME records (farcoreleft), so the recordings are made again
  (`python tools/rp.py check OUT --session NAME`, then copy OUT/rec/RECORD.OUT here).
- A session's `setup` in sessions.toml: `tools/u6save.py` commands (teleport, give, npc, set)
  applied to the character's saved game, the stage every run of that session starts from, in
  `build/dos/stage-NAME` (`python tools/rp.py stage NAME` makes and shows it). Replaying its
  recording takes `--session NAME` too, for the same stage. A test set-up goes through the saved
  game rather than a cheat at run time, so DOS and the port start from the same files.
- `dosbox-x.conf`: the DOSBox-X configuration of every recording and replay (exhume.toml's
  `[replay] dosbox_conf`). It is `exhume/tools/replaydos.mjs`'s own default, as a file: js-dos's
  hardware (a Sound Blaster 16 with an OPL3, an MPU-401, 16 MB), no sound output, the dynamic
  core at a fixed 300000 cycles with turbo, so a replay runs as fast as the host can. replaydos
  starts DOSBox-X with `-defaultconf`, so no user configuration is read. `tools/rp.py record`
  runs the same file with the same switches and the same kind of start-up batch, so that a
  recording and its replays load the program at the same segment (the user's DOSBox-X settings,
  or another start-up, moved it by a paragraph).
  Its `[dos]` settings load the program where js-dos does, with the same environment block
  (docs/replay.md, "js-dos and DOSBox-X").

Nothing from the game is in this folder: the saved game is made from your copy of the game by
the real U.EXE at every run (or taken from the cache).

## The character: U.EXE, scripted

`tools/rp.py chargen` (and every `record`, `replay` and `check` before it runs) copies your game
directory to a scratch C:, runs **U.EXE** there in DOSBox-X with the keys of `[_chargen]`, and
keeps the `SAVEGAME/OBJLIST` it writes in `build/dos/stage/` (beside the replay build, which a rebuild empties), which every run then copies
over the game. It is cached by a sha1 over its inputs (U.EXE, your OBJLIST, the key program, the
DOSBox-X configuration and executable) and checked against `objlist_sha1`: any other result fails.

The keys go through the gypsy's questions, the name and the portrait screen: Escape (the
introduction), C (create), the name `Avatar`, M (male), C (create, with the first portrait),
the woods' and the gypsy's passages, **A** to every one of the seven questions, Escape through
the rest and the moongate introduction. U.EXE then runs GAME.EXE, which in that scratch copy is
a 63-byte program of ours (`rp.py`'s `STUB_EXE`) that exits.

The character: **Avatar, male, the first male portrait** (OBJLIST 1C72h = 7), strength 17,
dexterity 20, intelligence 22 (magic 44), level 3, 370 experience, 90 hit points, karma 75.
OBJLIST sha1 `01572292630827a62cfdfabb0018f7db2f2b2951` (7,539 bytes: U.EXE writes the first
1C73h bytes over the installed file, which is longer).

### Why it is the same every time

U.EXE is not deterministic by itself: `main` seeds rand() with `srand(time(0))`, the gypsy's
questions are shuffled with rand() (`Shuffle`, chargen.c), and from the first answer on her
pattern animation calls rand() once a frame (`DrawPattern`), so the second round's pairs depend on
how many frames passed before each key. Answer A always picks the lower virtue of a pair and B
the higher, but no fixed answers give the same strength, dexterity and intelligence whatever the
pairs (each virtue's gains differ, and the round winners depend on the pairing).

So the keys are typed by `tools/keyfeed.py`'s KEYFEED.COM, a TSR of ours loaded before U.EXE,
in mode `dos`: it answers Turbo C's `kbhit` (int 21h 0Bh) "no key" until it has been asked N
times since the last key (`KEY/N`, default `gap`), then gives the key to `getch` (int 21h 07h);
and it answers `getdate`/`gettime` (int 21h 2Ah, 2Ch) with `date` and `time`. The seed is then
fixed, and every key arrives in the same frame on every host: the same OBJLIST came out at fixed
300000 cycles with turbo, at fixed 60000 without, and at `cycles=max`; a different `time` gives a
different character (strength 18, intelligence 21 at 12:00:07), so the clock really is the seed.

## The keys of a session: KEYFEED.COM, not AUTOTYPE

A session with `feed` in sessions.toml is typed the same way, by KEYFEED.COM in mode `bios`
(int 16h functions 1/11h and 0/10h, which GAME's `KBD_GetKey` uses, N polls apart), loaded high
(`LH`) so that conventional memory is as in a replay. It also gives GAME the fixed clock, so
`OSI_timeSeed` (RAND.ASM, int 21h 2Ch) makes the same seed: a recording of a script is the same
file every time (the smoke session: five recordings, one RECORD.OUT sha1). It can type what
AUTOTYPE cannot (ctrl+s, the game's save). Two masks in exhume.toml cover the two vectors it
hooks, which a replay does not have.

Recordings typed by DOSBox-X's AUTOTYPE (a session without `feed`) hung now and then: 3 of 37
recordings of the smoke session here (2026-11-26), as in tasks 6 and 7. Two kinds, both in
DOSBox-X, with a trace (`UWRPTRACE=0,1`) and DOSBox-X's `-time-limit 200` for a clean stop that
flushes the files:
- the game was not hung: after its 20th key it went on polling the keyboard (2,484 KEY hook
  calls, all 0, with the mouse polls between) for the remaining 100 s, but none of the last six
  keys, F9 among them, ever reached the BIOS buffer;
- DOSBox-X itself stopped: it ignored its own `-time-limit` and was killed at 260 s.

AUTOTYPE types from a host thread of its own, in real time; what goes wrong inside DOSBox-X was
not traced further. Neither the game nor the harness is involved in what was seen: the game
kept polling, and the harness only reads the BIOS through the game's own int 16h calls. The earlier
reading, that a comment block at the top of this conf caused it, was a coincidence of two
hangs; it did expose a real bug, now fixed: `rp.py record` replaced the first `[cpu]` in the
file's text, so a comment mentioning `[cpu]` received `cycles=max` and the real section lost
its cycles (it now edits the section, `conf_set`). With KEYFEED, the smoke session with such a
comment block recorded five times out of five.
