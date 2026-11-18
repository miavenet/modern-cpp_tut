#!/bin/bash
# Check for M5-C5: chrono literals namespace (compile-verified)
# Learner's answer on stdin. Blank: chrono_literals (also chrono, literals,
# or literals::chrono_literals). The 100ms literal only resolves if the
# right inline namespace is opened.
answer=$(cat)

b1=$(printf '%s' "$answer" | xargs)
b1=${b1#std::}
case "$b1" in
  chrono_literals|literals|literals::chrono_literals|chrono) ;;
  *) echo 'MISS: use a namespace that exposes the chrono literals'; exit 1 ;;
esac

[ -z "$b1" ] && { echo "MISS: blank (chrono_literals namespace) not identified"; exit 1; }

if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing'; exit 2; fi
tmp=$(mktemp -d) || {
    echo 'SETUP PROBLEM: cannot create a temporary build directory'
    exit 2
}
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >/dev/null 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'
    exit 2
fi
cat > "$tmp/t.cpp" <<CPP
#include <chrono>
using namespace std::$b1;
int main() {
    auto timeout = 100ms;
    (void)timeout;
}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
