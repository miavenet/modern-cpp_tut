#!/bin/bash
# Check for M3-C2: release/acquire memory orderings (compile-verified)
# Learner's answer on stdin. Blanks: 1) release (store) 2) acquire (load).
# relaxed/seq_cst also compile but are caught by the card RejectIf guard.
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*release[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*acquire[[:space:]]*$ ]] || {
    echo "MISS: answer exactly release, acquire in that order"
    exit 1
}
b1=release
b2=acquire

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
#include <atomic>
#include <string>
#include <iostream>
std::atomic<bool> flag{false};
std::string msg_buffer;
void producer() {
    msg_buffer = "hello";
    flag.store(true, std::memory_order_$b1);
}
void consumer() {
    while (!flag.load(std::memory_order_$b2)) {}
    std::cout << msg_buffer;
}
int main() {}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
