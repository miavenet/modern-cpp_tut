#!/bin/bash
# Check for M3-C3: atomic_ref completion (compile-verified)
# Learner's answer on stdin. Blank: atomic_ref (atomic ops over a plain int).
answer=$(cat)

[[ "$answer" =~ ^[[:space:]]*atomic_ref[[:space:]]*$ ]] || { echo "MISS: answer exactly the atomic_ref blank"; exit 1; }
b1=atomic_ref

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
struct LegacyData { int counter; };
int main() {
    alignas(std::atomic_ref<int>::required_alignment) LegacyData data{};
    std::$b1<int> atomic_counter(data.counter);
    atomic_counter.fetch_add(1, std::memory_order_relaxed);
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
