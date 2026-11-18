#!/bin/bash
# Check for M1-C6: monotonic clock completion (compile-verified)
# Learner's answer on stdin. Extracts the clock name they chose and compiles
# std::chrono::<clock>::now(). A made-up clock fails to compile => MISS.
# (steady_clock is correct; system_clock/high_resolution_clock compile but are
#  caught by the card's RejectIf contradiction guard.)
answer=$(cat)

b1=$(echo "$answer" | grep -oE 'steady_clock|system_clock|high_resolution_clock|[a-z_]+_clock' | head -1)

[ -z "$b1" ] && { echo "MISS: blank (chrono clock type) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <chrono>
int main() {
    auto start = std::chrono::$b1::now();
    auto end   = std::chrono::$b1::now();
    (void)start; (void)end;
}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
