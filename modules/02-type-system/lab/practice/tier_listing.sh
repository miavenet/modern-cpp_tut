#!/bin/bash
# tier_listing.sh - run tier_table.cpp, then print the -O2 listing of by_rule and by_table.
# Run from the tutorial root: bash modules/02-type-system/lab/practice/tier_listing.sh clang++
CXX=${1:-clang++}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
$CXX -std=c++20 -O2 -Wall -Wextra "$D/tier_table.cpp" -o "$W/tier_table" && "$W/tier_table"
$CXX -std=c++20 -O2 -S "$D/tier_table.cpp" -o "$W/tier_table.s"
for fn in by_rule by_table; do
  echo "--- $fn"
  awk -v fn=$fn '$0 ~ "^_?_Z[0-9]+"fn && /:/ {on=1; sym=$1; sub(/:.*/,"",sym); print "symbol " sym; next} on && /cfi_endproc|^_?_Z|\.size/ {on=0} on && !/^[ \t]*[.;#]/ && !/^[._A-Za-z0-9$]*:/ {print}' "$W/tier_table.s" | sed 's/[;#] *=.*//; s/[ \t]*\/\/.*//'
done
rm -f "$W/tier_table.s"
