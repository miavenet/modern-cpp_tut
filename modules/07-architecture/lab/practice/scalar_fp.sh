#!/bin/bash
# scalar_fp.sh - cross-compile scalar_fp.cpp for x86-64 with -mavx2 (no sysroot needed) and count register names and arithmetic kinds.
# Run from the tutorial root: bash modules/07-architecture/lab/practice/scalar_fp.sh clang++
CXX=${1:-clang++}
D=$(dirname "$0"); W=${TMPDIR:-/tmp}
$CXX --version | head -1
$CXX -std=c++20 -O2 -Wall -Wextra --target=x86_64-linux-gnu -mavx2 -S "$D/scalar_fp.cpp" -o "$W/scalar_fp.s" -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize 2>&1 | grep -E 'remark|warning|error' | sed 's/; allow reordering.*//' | sort | uniq
for fn in _Z5totalPKdm _Z5twicePdm; do
  sed -n "/^$fn:/,/\.cfi_endproc/p" "$W/scalar_fp.s" | awk -v fn=$fn '
    /^[ \t]*[.#]/ || /^[._A-Za-z0-9]*:/ {next}
    /%[xyz]mm[0-9]/ {x++} /%ymm[0-9]/ {y++}
    /^[ \t]*v?[a-z0-9]*p[sd][ \t]/ {anyp++; if ($1 !~ /^v?(add|mul|sub|div)p[sd]$/) others[$1]++}
    /^[ \t]*v?(add|mul|sub|div)sd[ \t]/ {sd++} /^[ \t]*v?(add|mul|sub|div)pd[ \t]/ {pd++}
    END {printf "%-14s xmm/ymm/zmm lines: %2d | ymm lines: %2d | any mnemonic ending ps/pd: %2d | scalar-double arithmetic: %d | packed-double arithmetic (add/mul/sub/div+pd): %d\n", fn, x, y, anyp, sd, pd
         for (m in others) printf "   non-arithmetic ps/pd mnemonic: %s x%d\n", m, others[m]}'
done
rm -f "$W/scalar_fp.s"
