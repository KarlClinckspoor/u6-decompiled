#!/bin/bash
# The port built with AddressSanitizer and UndefinedBehaviorSanitizer (clang), for replaying the
# sessions under them (docs/port/PORT.md, "Sanitizers"). Run on Linux or in WSL:
#
#     bash tools/sanbuild.sh [BUILD_DIR]          # default ~/u6port-asan
#
# It writes BUILD_DIR/u6 and a wrapper, BUILD_DIR/u6-san, that runs it with the sanitizers'
# options: keep going after a report (-fsanitize-recover=all), no leak report (the port frees
# nothing at exit), a stack with each report. From Windows, rp.py runs the wrapper through WSL
# as the Linux port:
#
#     U6_PORT_WSL='~/u6port-asan/u6-san' python tools/rp.py port-check OUT --rec R --session S --variant linux-clang
#
# (on Linux itself, U6_PORT=BUILD_DIR/u6-san). The reports go to the run's port.log.
set -e
SRC="$(cd "$(dirname "$0")/.." && pwd)/port"
B=${1:-$HOME/u6port-asan}
FLAGS="-fsanitize=address,undefined -fsanitize-recover=all -fno-omit-frame-pointer -g"
cmake -S "$SRC" -B "$B" -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" >/dev/null
cmake --build "$B" -j4 2>&1 | grep -E "error" || true
test -x "$B/u6"
cat > "$B/u6-san" <<'EOF'
#!/bin/bash
export ASAN_OPTIONS=halt_on_error=0:detect_leaks=0:abort_on_error=0:print_summary=1
export UBSAN_OPTIONS=halt_on_error=0:print_stacktrace=1:print_summary=1
# address-space randomisation off: clang 14's ASan crashes at random at start-up, before it can
# report anything, on kernels whose mmap randomisation has 32 bits (WSL's 6.6: 2 runs in 5)
exec setarch "$(uname -m)" -R "$(dirname "$0")/u6" "$@"
EOF
chmod +x "$B/u6-san"
echo "built $B/u6 (run $B/u6-san)"
