#!/bin/bash
# Check for M6-C4: complete the __builtin_prefetch call for a one-shot READ
# probe (rw=0, locality=0). Learner's answer on stdin.
# Blanks: 1) the builtin name  2) rw arg = 0 (read)  3) locality arg = 0 (no
# retain / streaming). The wrapper validates the whole input and forwards
# exactly these three ordered tokens.
answer=$(cat)
IFS=, read -r builtin rw loc <<< "$answer"
builtin=${builtin//[[:space:]]/}
rw=${rw//[[:space:]]/}
loc=${loc//[[:space:]]/}
if [ "$builtin" != '__builtin_prefetch' ]; then
    echo "MISS: expected __builtin_prefetch; got $builtin"; exit 1
fi

if [ "$rw" != "0" ]; then
    echo "MISS: rw (2nd arg): expected 0 for READ; got ${rw:-missing} (1 = write)"
    exit 1
fi
if [ "$loc" != "0" ]; then
    echo "MISS: locality (3rd arg): expected 0 for a one-shot probe; got ${loc:-missing}"
    exit 1
fi

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <cstddef>
static int buckets[1024];
int main() {
    size_t h = 7;
    __builtin_prefetch(&buckets[h], $rw, $loc);
    return buckets[h];
}
CPP

if err=$(clang++ -std=c++20 -Wall -O2 -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in prefetch call compiles: rw=$rw read, locality=$loc streaming)"
    exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
