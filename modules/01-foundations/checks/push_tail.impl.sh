#!/bin/bash
# Check for M1-LPA2 (LPA push_back-tail lab). Learner's answer on stdin: the path to
# their push_tail.cpp. Grades only deterministic counters and type traits, never a time:
#   1. the program builds and prints RESULT: PASS (growth moves instead of copying;
#      the prepared book made no copies and no heap calls while timed);
#   2. a probe checks Order is nothrow move constructible and calls prepare_for_peak
#      directly with OTHER peaks: the book is left empty, and pushing exactly `peak`
#      moved orders afterwards makes zero global operator new calls and zero copies.
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
hdr="$root/modules/06-hft-patterns/lab/measure"
if [ ! -f "$hdr/lat_hist.hpp" ]; then echo 'SETUP PROBLEM: lat_hist.hpp is missing from module 06'; exit 2; fi
if ! clang++ -std=c++20 -Wall -Wextra -O1 -I "$hdr" "$source_path" -o "$tmp/tail" >"$tmp/build.log" 2>&1; then
    echo 'MISS: push_tail.cpp does not compile'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
"$tmp/tail" >"$tmp/tail.out" 2>&1
if [ $? -ne 0 ] || ! grep -q '^RESULT: PASS' "$tmp/tail.out"; then
    if grep -q 'move-growth still copies' "$tmp/tail.out"; then
        echo "MISS: std::vector still copies Order when it grows (is Order's move constructor noexcept?)"
    else
        echo 'MISS: the prepared book still made heap calls or copies while the clock ran'
    fi
    grep '^RESULT:' "$tmp/tail.out" | head -1
    exit 1
fi
cat >"$tmp/probe.cpp" <<CPP
#define PUSH_TAIL_NO_MAIN
#include "$source_path"
static_assert(std::is_nothrow_move_constructible_v<Order>, "Order's move must be noexcept");
static bool tight(std::size_t peak) {
    std::vector<Order> src;
    src.reserve(peak);
    for (std::size_t i = 0; i < peak; ++i) src.emplace_back(i, 1.0, std::string(40, 'x'));
    std::vector<Order> book;
    prepare_for_peak(book, peak);
    if (!book.empty()) return false;
    // Graded on the vector itself, not on the lab's counters (which a file could edit):
    // the buffer must not move or grow while pushing exactly peak orders.
    const std::size_t cap = book.capacity();
    const Order* const first = book.data();
    for (std::size_t i = 0; i < peak; ++i) book.push_back(std::move(src[i]));
    return cap >= peak && book.capacity() == cap && book.data() == first && book.size() == peak;
}
int main() { return (tight(777) && tight(5000) && tight(1) && tight(kPeak + 10000)) ? 0 : 1; }
CPP
if ! clang++ -std=c++20 -O1 -I "$hdr" "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    if grep -q "move must be noexcept" "$tmp/build.log"; then
        echo "MISS: Order's move constructor is not noexcept"
    else
        echo 'MISS: prepare_for_peak does not compile against the probe'; sed -n '1,3p' "$tmp/build.log"
    fi
    exit 1
fi
if ! "$tmp/probe"; then
    echo 'MISS: prepare_for_peak must leave the book empty with room for any peak it is given (no heap call, no copy while pushing)'
    exit 1
fi
echo 'HIT (growth moves; prepared book: zero heap calls and zero copies while timed; prepare_for_peak works for other peaks)'
