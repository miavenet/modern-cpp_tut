#!/bin/bash
# Check for M2-C1: custom concept completion (compile-verified)
# Learner's answer on stdin. Blanks: 1) concept 2) requires 3) double.
# Fills the concept and a static_assert against a model type that satisfies it.
answer=$(cat)

b1=$(echo "$answer" | grep -oiE 'concept' | head -1)
b2=$(echo "$answer" | grep -oiE 'requires' | head -1)
b3=$(echo "$answer" | grep -oiE 'double' | head -1)

[ -z "$b1" ] && { echo "MISS: blank 1 (concept keyword) not identified"; exit 1; }
[ -z "$b2" ] && { echo "MISS: blank 2 (requires) not identified"; exit 1; }
[ -z "$b3" ] && { echo "MISS: blank 3 (double return type) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <concepts>
template <typename T>
$b1 OrderType = $b2 (T t) {
    { t.price() } -> std::same_as<$b3>;
};
struct Ord { double price() const { return 1.0; } };
static_assert(OrderType<Ord>);
int main() {}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
