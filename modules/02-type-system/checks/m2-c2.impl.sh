#!/bin/bash
# Check for M2-C2: consteval completion (compile-verified)
# Learner's answer on stdin. Blank: consteval (forces compile-time evaluation).
# Only 'consteval' is accepted here (constexpr is caught by the card RejectIf);
# the function is used as an array bound so a non-immediate keyword would still
# need to be a valid function specifier.
answer=$(cat)

b1=$(printf '%s' "$answer" | sed -E 's/^[[:space:]]+//;s/[[:space:]]+$//')

[[ "$b1" == consteval ]] || { echo "MISS: the single blank must be consteval"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <array>
$b1 int getTableSize(int base) {
    int size = base;
    for (int i = 0; i < 4; ++i) { size *= 2; }
    return size;
}
std::array<int, getTableSize(8)> table;
int main() { (void)table.size(); }
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
