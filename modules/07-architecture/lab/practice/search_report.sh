#!/bin/bash
# search_report.sh - vectorizer report and census for search_fixed.cpp.
# Run from the tutorial root: bash modules/07-architecture/lab/practice/search_report.sh clang++   (or: g++ O3)
CXX=${1:-clang++}; OPT=${2:-O2}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
if $CXX --version | grep -qi clang; then REM="-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize"; else REM="-fopt-info-vec-optimized -fopt-info-vec-missed"; fi
echo "=== search_fixed.cpp -$OPT: compiler report"
$CXX -std=c++20 -$OPT -Wall -Wextra -S "$D/search_fixed.cpp" -o "$W/search_fixed.s" $REM 2>&1 | grep -E 'remark|warning|error|optimized:|missed:' | sort | uniq
echo "=== census"; bash "$D/census.sh" "$W/search_fixed.s"; rm -f "$W/search_fixed.s"
