# Ultima VI decompilation: the one-command gates (run in WSL/Linux; `make check` is also
# what `python3 tools/gate.py check` does, and that runs in Git Bash on Windows too).
#
#   make check   rebuild GAME.EXE, U.EXE and END.EXE with the original tools in a headless
#                DOSBox and require each to be identical to the original (tools/gate.py)
#   make port    build the native port (cmake, port/ -> build/)
#   make test    check + port + the port tests that run headless
#   make help
#
# Needs for `check`: python3, the originals in gamedata/, the toolchain in tools/turboc20 and
# tools/masm300, a DOSBox-X (tools/gate.py finds one and sets up the snap accommodations).
# Needs for `port`: cmake, a C compiler, SDL2 development files.
# Needs for `test` also: python3 with unicorn (`pip install unicorn`).

PYTHON   ?= python3
GATE      = $(PYTHON) tools/gate.py
BUILD    ?= build
DOSOUT    = $(BUILD)/dos
CMAKE    ?= cmake
JOBS     ?= $(shell nproc 2>/dev/null || echo 2)
# QUICK=1 runs the diff tests with --quick
QUICKFLAG = $(if $(QUICK),--quick,)

export U6_GAMEDATA ?= $(CURDIR)/gamedata

.PHONY: help check port test

help:
	@echo "make check   rebuild GAME.EXE, U.EXE, END.EXE with the original tools; must be identical to the originals"
	@echo "make port    build the native port into $(BUILD)/ (cmake -S port -B $(BUILD))"
	@echo "make test    check (keeping the build in $(DOSOUT)/), port, then the headless port tests:"
	@echo "             OSI unit tests, osilib_diff, mcga_diff, ugfx_diff, uosi_diff, adlib_diff (need unicorn)"
	@echo "             QUICK=1 for the diff tests' --quick; PYTHON=... to choose the interpreter"
	@echo "Not yet: verify, golden, fuzz (see the marked place in this Makefile)"

check:
	@$(GATE) check

port:
	$(CMAKE) -S port -B $(BUILD) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD) -j $(JOBS)

# the gate again, keeping the DOS builds: osilib_diff runs the original routines from
# $(DOSOUT)/game/GAME.EXE (+ GAME.MAP)
test: export U6_DOS_EXE = $(CURDIR)/$(DOSOUT)/game/GAME.EXE
test:
	@$(PYTHON) -c "import unicorn" 2>/dev/null || { echo "make test: python3 module 'unicorn' is missing for $(PYTHON) (pip install unicorn, or PYTHON=...)"; exit 2; }
	@$(GATE) check --keep $(DOSOUT)
	$(MAKE) port
	PATH="$$(dirname "$$(command -v $(PYTHON))"):$$PATH" sh port/tests/run_osilib_tests.sh $(QUICKFLAG)
	$(PYTHON) port/tests/mcga_diff.py $(QUICKFLAG)
	$(PYTHON) port/tests/ugfx_diff.py $(QUICKFLAG)
	$(PYTHON) port/tests/uosi_diff.py $(QUICKFLAG)
	$(PYTHON) port/tests/adlib_diff.py
	@echo "make test: all passed (replays need a recording and a display: not run here)"

# ---- future targets (not implemented) -------------------------------------------------
# golden   make each session's reference from DOS: replay it twice, require both identical,
#          store per-checkpoint digests (PLAN-exact-port.md, Phase 3)
# verify   replay every recorded session in the port and compare with its golden
# fuzz     single routines fuzzed against the original bytes in Unicorn, vectors kept
# ----------------------------------------------------------------------------------------
