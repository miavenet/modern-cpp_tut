#!/bin/bash
# clamp_sve.sh - compile clamp.cpp for AArch64 NEON, then with SVE, and count stores and branches per function.
# Run from the tutorial root: bash modules/05-memory-perf/lab/practice/clamp_sve.sh clang++
CXX=${1:-clang++}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
for T in "--target=aarch64-linux-gnu" "--target=aarch64-linux-gnu -march=armv8-a+sve"; do
  echo "=== target: $T  (-O2)"
  $CXX -std=c++20 -O2 -Wall -Wextra $T -S "$D/clamp.cpp" -o "$W/clamp.s" -Rpass=loop-vectorize -Rpass-missed=loop-vectorize 2>&1 | grep -E 'remark|warning|error' | sed 's/ Use #pragma.*//' | sort | uniq
  bash "$D/clamp_census.sh" "$W/clamp.s"
done
rm -f "$W/clamp.s"
