#!/bin/bash
# Check for M1-LPA4: pre-size a hash table for its peak. Learner's answer on stdin: one
# C++ statement run on a std::unordered_map<std::uint64_t, Order> named `live` right
# after it is built. Graded by counters, never by time: after the statement, inserting
# 500,000 elements must not change bucket_count() (no rehash), for the default
# max_load_factor() of 1.0 and for 0.5. The statement must leave max_load_factor()
# unchanged and must not allocate more than 3x the buckets the peak needs. `reserve(500'000)` and a `rehash` sized as
# 500'000 / live.max_load_factor() pass; `rehash(500'000)` alone does not at 0.5.
set -u
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*;$//; s/[[:space:]]*$//')
if [ -z "$answer" ]; then echo 'MISS: give one C++ statement'; exit 1; fi
case "$answer" in
    *';'*|*'#'*|*'\'*) echo 'MISS: give one statement'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'; exit 2
fi
cat >"$tmp/probe.cpp" <<CPP
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
struct Order { std::uint64_t id; double px; int qty; char side; };
static int trial(float load_factor) {
    std::unordered_map<std::uint64_t, Order> live;
    live.max_load_factor(load_factor);
    $answer;
    if (live.max_load_factor() != load_factor) return 4;  // the answer must not change the load factor
    const std::size_t before = live.bucket_count();
    const std::size_t need = static_cast<std::size_t>(std::ceil(500'000 / load_factor));
    if (before > 3 * need) return 5;                      // far oversized for a 500,000 peak at this load factor
    for (std::uint64_t i = 0; i < 500'000; ++i) live.emplace(i, Order{i, 1.0, 1, 'B'});
    return live.bucket_count() == before ? 0 : 3;         // 3: an insert still rehashed
}
int main() {
    if (int r = trial(1.0f)) return r;
    return trial(0.5f);
}
CPP
if ! clang++ -std=c++20 -O1 "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that statement does not compile (the map is named live)'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
"$tmp/probe" >"$tmp/run.log" 2>&1
case $? in
    0) echo 'HIT (500,000 inserts caused no rehash, at max_load_factor 1.0 and 0.5)'; exit 0 ;;
    3) echo 'MISS: an insert before 500,000 elements still rehashed (rehash(n) sets buckets, not elements: divide by max_load_factor(), or use reserve)'; exit 1 ;;
    4) echo 'MISS: max_load_factor() was changed by the statement: size the table for the peak, do not loosen its load factor'; exit 1 ;;
    5) echo 'MISS: far more buckets than a 500,000 peak needs at this max_load_factor'; exit 1 ;;
    *) echo 'MISS: the program failed'; exit 1 ;;
esac
