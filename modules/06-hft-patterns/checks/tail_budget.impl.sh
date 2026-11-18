#!/bin/bash
# Check for M6-LPA5 (tail-budget capstone). Learner's answer on stdin: the path to
# their tail_budget.cpp. Grades the schedule's STRUCTURE, never a time: the whole
# program must build, and a probe with fixed numbers (no clock) checks that
# intended_send_ns(start, i, prev_done) == start + i * kIntervalNs for many i and for
# prev_done values early, late and absurd. A schedule that waits for the previous
# reply, paces from "now", or starts one interval late fails.
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
source_path=$(head -n 1 | sed "s/^[[:space:]]*//; s/[[:space:]]*\$//; s/^[\"'\`]//; s/[\"'\`]\$//; s/^[[:space:]]*//; s/[[:space:]]*\$//")
case "$source_path" in /*) ;; *) source_path="$root/$source_path" ;; esac
if [ ! -f "$source_path" ]; then echo "MISS: workspace file not found: $source_path"; exit 1; fi
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'; exit 2
fi
lab="$root/modules/06-hft-patterns/lab"
if [ ! -f "$lab/measure/lat_hist.hpp" ] || [ ! -f "$lab/spsc/solution/spsc_ring.hpp" ]; then
    echo 'SETUP PROBLEM: lat_hist.hpp or the reference spsc_ring.hpp is missing from module 06'; exit 2
fi
inc=(-I "$lab/measure" -I "$lab/spsc/solution")
if ! clang++ -std=c++20 -Wall -Wextra -O1 "${inc[@]}" "$source_path" -o "$tmp/full" >"$tmp/build.log" 2>&1; then
    echo 'MISS: tail_budget.cpp does not compile'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
cat >"$tmp/probe.cpp" <<CPP
#define TAIL_BUDGET_NO_MAIN
#include "$source_path"
int main() {
    const std::uint64_t starts[] = {0, 5'000, 123'456'789};
    const std::uint64_t is[] = {0, 1, 2, 777, 999'999};
    for (std::uint64_t s : starts)
        for (std::uint64_t i : is) {
            const std::uint64_t want = s + i * 2'000;
            const std::uint64_t prevs[] = {0, want / 2, want + 300'000, std::uint64_t{1} << 40};
            for (std::uint64_t p : prevs)
                if (intended_send_ns(s, i, p) != want) return 1;
        }
    // Pseudo-random triples in a scrambled order, twice: a lookup table, or a schedule that
    // keeps state between calls (pacing from the previous reply), cannot match.
    std::uint64_t x = 88172645463325252ull;
    for (int round = 0; round < 2; ++round) {
        std::uint64_t y = x;
        for (int k = 0; k < 400; ++k) {
            y ^= y << 13; y ^= y >> 7; y ^= y << 17;
            const std::uint64_t s = y >> 30, i = (y >> 3) & 0xFFFFFFFFull, p = y * 31 + (k % 3 ? 0 : s);
            if (intended_send_ns(s, i, p) != s + i * 2'000) return 1;
        }
    }
    return kIntervalNs == 2'000 ? 0 : 2;
}
CPP
if ! clang++ -std=c++20 -O1 "${inc[@]}" "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: intended_send_ns does not compile against the probe'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
"$tmp/probe"
case $? in
    0) echo 'HIT (open loop: message i is due at start + i * interval, whatever the previous reply did)'; exit 0 ;;
    2) echo 'MISS: keep kIntervalNs at 2,000 ns; change the schedule, not the feed rate'; exit 1 ;;
    *) echo 'MISS: intended_send_ns must be start + i * kIntervalNs, never paced from the previous reply or from now'; exit 1 ;;
esac
