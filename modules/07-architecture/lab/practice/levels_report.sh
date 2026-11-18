#!/bin/bash
# levels_report.sh - for each -O level: how many report lines the vectorizer prints, and what the code is.
# Run from the tutorial root: bash modules/07-architecture/lab/practice/levels_report.sh clang++   (or: g++)
CXX=${1:-clang++}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
if $CXX --version | grep -qi clang; then REM="-Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize"; PAT='remark:'; else REM="-fopt-info-vec-optimized -fopt-info-vec-missed"; PAT='optimized:|missed:'; fi
for O in O0 O1 Os O2 O3; do
  $CXX -std=c++20 -$O -Wall -Wextra -S "$D/levels.cpp" -o "$W/levels.s" $REM > "$W/levels.txt" 2>&1
  echo "=== -$O : report lines = $(grep -cE "$PAT" "$W/levels.txt")"
  grep -E "$PAT|warning" "$W/levels.txt" | sed 's/ Use #pragma.*//' | sort | uniq
  bash "$D/census.sh" "$W/levels.s"
done
rm -f "$W/levels.s" "$W/levels.txt"
