#!/bin/bash
# Check for M1-C5: scoped 1-byte enum completion (compile-verified)
# Learner's answer on stdin. Extracts the scope keyword and underlying type,
# fills the template, and a static_assert enforces the 1-byte requirement.
# Blanks: 1) class 2) uint8_t / int8_t / char
answer=$(cat)

b1=$(echo "$answer" | grep -oiE 'class|struct' | head -1)            # scope keyword
b2=$(echo "$answer" | grep -oE 'uint8_t|int8_t|unsigned char|signed char|char' | head -1)  # 1-byte underlying

[ -z "$b1" ] && { echo "MISS: blank 1 (class scope keyword) not identified"; exit 1; }
[ -z "$b2" ] && { echo "MISS: blank 2 must be a 1-byte type; int is not 1 byte"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <cstdint>
enum $b1 OrderType : $b2 { Market, Limit, Stop };
static_assert(sizeof(OrderType) == 1, "OrderType must fit in exactly 1 byte");
int main() { OrderType t = OrderType::Market; (void)t; }
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
