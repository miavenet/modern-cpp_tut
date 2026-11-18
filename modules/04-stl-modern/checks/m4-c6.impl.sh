#!/bin/bash
# Check for M4-C6: std::span<const std::byte> completion (compile-verified)
# Learner's answer on stdin. Blanks: 1) span 2) byte.
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*span[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*byte[[:space:]]*$ ]] || {
    echo "MISS: answer exactly span, byte in that order"
    exit 1
}
b1=span
b2=byte

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
#include <span>
#include <cstddef>
#include <vector>
void processPacket(std::$b1<const std::$b2> buffer) {
    (void)buffer;
}
int main() {
    std::vector<std::byte> rx_buf(2048);
    processPacket(rx_buf);
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
