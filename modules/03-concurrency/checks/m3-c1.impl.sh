#!/bin/bash
# Check for M3-C1: auto-joining cancellable thread (compile-verified)
# Learner's answer on stdin. Blanks: 1) jthread 2) stop_token 3) stop_requested.
answer=$(cat)

IFS=, read -r b1 b2 b3 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*jthread[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*stop_token[[:space:]]*$ &&
   "$b3" =~ ^[[:space:]]*stop_requested[[:space:]]*$ ]] || {
    echo "MISS: answer exactly jthread, stop_token, stop_requested in that order"
    exit 1
}
b1=jthread
b2=stop_token
b3=stop_requested

tmp=$(mktemp -d) || {
    echo 'SETUP PROBLEM: cannot create temporary build directory'
    exit 2
}
trap 'rm -rf "$tmp"' EXIT
printf 'int main() { return 0; }\n' > "$tmp/probe.cpp"
if ! clang++ -std=c++20 -Wall -Wextra "$tmp/probe.cpp" -o "$tmp/probe" >/dev/null 2>&1 || ! "$tmp/probe"; then
    echo 'SETUP PROBLEM: compiler or runtime probe failed'
    exit 2
fi
cat > "$tmp/t.cpp" <<CPP
#include <thread>
void processItem();
void backgroundProcessor() {
    std::$b1 worker([](std::$b2 st) {
        while (!st.$b3()) {
            processItem();
        }
    });
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -c -o "$tmp/t.o" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
