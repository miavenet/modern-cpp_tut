#!/bin/bash
# calib_report.sh - vectorizer report and store/branch census for calib.cpp, then the output comparison.
# Run from the tutorial root: bash modules/05-memory-perf/lab/practice/calib_report.sh clang++   (or: g++ O3)
CXX=${1:-clang++}; OPT=${2:-O2}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
if $CXX --version | grep -qi clang; then REM="-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize"; else REM="-fopt-info-vec-optimized -fopt-info-vec-missed"; fi
echo "=== calib.cpp -$OPT: compiler report"
$CXX -std=c++20 -$OPT -Wall -Wextra -S "$D/calib.cpp" -o "$W/calib.s" $REM 2>&1 | grep -E 'remark|warning|error|optimized:|missed:' | sort | uniq
echo "=== census"; bash "$D/census.sh" "$W/calib.s"; rm -f "$W/calib.s"
echo "=== output comparison"
$CXX -std=c++20 -$OPT -Wall -Wextra "$D/calib.cpp" "$D/calib_main.cpp" -o "$W/calib_main" && "$W/calib_main"
