# Replay sessions

What the replay DOS build's runs read (docs/replay.md):

- `sessions.toml`: key scripts for `tools/rp.py record` and `check`.
- `dosbox-x.conf`: the DOSBox-X configuration of every recording and replay (exhume.toml's
  `[replay] dosbox_conf`). It is `exhume/tools/replaydos.mjs`'s own default, as a file: js-dos's
  hardware (a Sound Blaster 16 with an OPL3, an MPU-401, 16 MB), no sound output, the dynamic
  core at a fixed 300000 cycles with turbo, so a replay runs as fast as the host can. replaydos
  starts DOSBox-X with `-defaultconf`, so no user configuration is read. `tools/rp.py record`
  runs the same file with the same switches and the same start-up batch, but with `cycles=max`
  and turbo off. That way a recording and its replays load the program at the same segment (the
  user's DOSBox-X settings, or another start-up batch, moved it by a paragraph), and the keys are
  typed in real time, the game as fast as the host runs it (a dump writes half a megabyte at each
  key).
  Keep the file free of comments. With a comment block at its top, every recording of the smoke
  session hung at the conversation's start (twice, at the same byte of STATE.OUT); without one, it
  did not (the cause wasn't looked into).
- `stage/`: copied over the game directory in every run, so every session starts from it.

## stage/SAVEGAME/OBJLIST: a saved game with a character

The game directory's own `SAVEGAME/` is the game as installed: its OBJLIST has no character, and
GAME.EXE then only tells you to make one with U.EXE. This OBJLIST (7,283 bytes) has one: an
Avatar named AVATAR, male, portrait 1, in the game's starting position.
It is the only file staged; the OBJBLK files and everything else come from your own copy of the
game.

It was made by the port's test switch `--new-avatar`, which writes the character into the
installed OBJLIST the way U.EXE's character creation does (port/ui/newchar.c, deterministic), and
copied out of the stage folder of the port's recording, `DIR/stage/OBJLIST`. In WSL/Linux, after
`make port`:

    SDL_VIDEODRIVER=dummy ./build/u6 --data gamedata --home /tmp/u6s/home --game \
        --new-avatar AVATAR,M,1 --hidden --no-audio --record /tmp/u6s/rec --run-ms 5000
    cp /tmp/u6s/rec/stage/OBJLIST tests/replay/stage/SAVEGAME/OBJLIST

That gives this file byte for byte (checked 2026-11-26).

Provenance, to be honest about: it is not all ours. Most of its bytes are the installed game's
starting OBJLIST (the party's and the other characters' records), with the new character's name,
portrait and statistics written in. It is kept because it is small and the sessions need it;
nothing else from the game is in the repository.

sha1 `0e6eaf34d9e1002f31dec7958474fc07ccd7d3e7`.
