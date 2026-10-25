#!/bin/sh
# Builds and runs the OSILIB tests (port/sys against the original routines):
#   test_osi_unit     unit tests: the DOS file system layer, the OSI file error protocol,
#                   OSI_delay and the timer under record/replay, dosmem, dosint
#   osilib_diff.py    differential tests: GAME.EXE's routines in unicorn vs port/sys
# Needs: cc (clang or gcc), python3 with unicorn; GAME.EXE of this source's DOS build
# (U6_DOS_EXE, default /home/claude/bld/dos/B/GAME.EXE) and the game data (U6_GAMEDATA).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
PORT=$(cd "$HERE/.." && pwd)
OUT="$HERE/build"
CC=${CC:-cc}
mkdir -p "$OUT"
SRCS="$PORT/sys/osi.c $PORT/sys/kbd.c $PORT/sys/lzw.c $PORT/sys/mouse.c $PORT/sys/rand.c \
$PORT/sys/sound.c $PORT/sys/timer.c $PORT/sys/dosint.c $PORT/sys/dosfs.c $PORT/sys/dosmem.c \
$PORT/gfx/vgadac.c $PORT/replay/replay.c $HERE/osi_stubs.c"
CFLAGS="-std=c99 -O1 -g -Wall -Wextra"
$CC $CFLAGS -fPIC -shared -o "$OUT/libosi_test.so" $SRCS "$HERE/osi_testlib.c"
$CC $CFLAGS -o "$OUT/test_osi_unit" $SRCS "$HERE/test_osi_unit.c"
"$OUT/test_osi_unit"
python3 "$HERE/osilib_diff.py" "$@"
