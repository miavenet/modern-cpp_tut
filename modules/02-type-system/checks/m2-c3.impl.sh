#!/bin/bash
# Check for M2-C3: fold expression + if constexpr completion (compile-verified)
# Learner's answer on stdin. Blanks: 1) args 2) if constexpr 3) T.
# The templates are instantiated for int, so 'if' (not 'if constexpr') would
# try to compile *val for a non-pointer and fail => the compile really tests it.
answer=$(cat)

b1=$(echo "$answer" | grep -oE 'args' | head -1)
b2=$(echo "$answer" | grep -oiE 'if constexpr' | head -1)
b3=$(echo "$answer" | grep -oE '\bT\b' | head -1)

[ -z "$b1" ] && { echo "MISS: blank 1 (args parameter pack) not identified"; exit 1; }
[ -z "$b2" ] && { echo "MISS: blank 2 (if constexpr) not identified"; exit 1; }
[ -z "$b3" ] && { echo "MISS: blank 3 (T type parameter) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <iostream>
#include <type_traits>
template<typename... Args>
void printAll(Args... $b1) {
    (std::cout << ... << $b1) << '\n';
}
template <typename $b3>
void maybeDereference($b3 val) {
    $b2 (std::is_pointer_v<$b3>) {
        std::cout << *val;
    } else {
        std::cout << val;
    }
}
template void printAll<int, int>(int, int);
template void maybeDereference<int>(int);
int main() {}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
