#!/bin/bash
# reuse_check.sh - plain, ASan, and ASan with quarantine off: how often does a freed address come straight back?
# Run from the tutorial root: bash modules/07-architecture/lab/practice/reuse_check.sh clang++   (or: g++)
# Prints the shim calls left in the probe's listing (plain, ASan), then reuse counts out of 257, 3 runs per build.
CXX=${1:-clang++}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
$CXX -std=c++20 -O2 -Wall -Wextra -c "$D/reuse_shim.cpp" -o "$W/reuse_shim_plain.o" || exit 1
$CXX -std=c++20 -O2 -Wall -Wextra -c "$D/reuse_probe.cpp" -o "$W/reuse_probe_plain.o" || exit 1
$CXX "$W/reuse_probe_plain.o" "$W/reuse_shim_plain.o" -o "$W/reuse_plain" || exit 1
$CXX -std=c++20 -O1 -g -fsanitize=address -Wall -Wextra -c "$D/reuse_shim.cpp" -o "$W/reuse_shim_asan.o" || exit 1
$CXX -std=c++20 -O1 -g -fsanitize=address -Wall -Wextra -c "$D/reuse_probe.cpp" -o "$W/reuse_probe_asan.o" || exit 1
$CXX -fsanitize=address "$W/reuse_probe_asan.o" "$W/reuse_shim_asan.o" -o "$W/reuse_asan" || exit 1
echo "## shim calls left in the probe listing (plain, then ASan):"
$CXX -std=c++20 -O2 -S "$D/reuse_probe.cpp" -o - | grep -cE '(bl|call|callq)[[:space:]]+_?_?(request_bytes|retire_bytes)'
$CXX -std=c++20 -O1 -g -fsanitize=address -S "$D/reuse_probe.cpp" -o - | grep -cE '(bl|call|callq)[[:space:]]+_?_?(request_bytes|retire_bytes)'
for i in 1 2 3; do echo -n "plain          run$i: "; "$W/reuse_plain"; done
for i in 1 2 3; do echo -n "asan default   run$i: "; "$W/reuse_asan"; done
for i in 1 2 3; do echo -n "asan quar=0    run$i: "; ASAN_OPTIONS=quarantine_size_mb=0 "$W/reuse_asan"; done
