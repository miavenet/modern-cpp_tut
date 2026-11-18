#!/bin/bash
# Check for M5-LPA2 (LPA pool-tail lab). Learner's answer on stdin: the path to
# their pool_tail.cpp. Grades only deterministic counters, never a time:
#   1. the program builds and prints RESULT: PASS (the prepared run made zero
#      heap calls while timed; the default and lazy runs made some);
#   2. a probe calls prepare_for_peak directly with OTHER peaks and checks the
#      book is left empty and that pushing exactly `peak` orders afterwards
#      makes zero global operator new calls.
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
src_dir=$(cd "$(dirname "$source_path")" && pwd)
if [ ! -f "$src_dir/pool_lab.cpp" ]; then
    echo "MISS: pool_lab.cpp must sit next to pool_tail.cpp (finish lesson 05's lab first)"; exit 1
fi
hdr="$root/modules/06-hft-patterns/lab/measure"
if [ ! -f "$hdr/lat_hist.hpp" ]; then echo 'SETUP PROBLEM: lat_hist.hpp is missing from module 06'; exit 2; fi
if ! clang++ -std=c++20 -Wall -Wextra -O1 -I "$hdr" "$source_path" -o "$tmp/tail" >"$tmp/build.log" 2>&1; then
    echo 'MISS: pool_tail.cpp does not compile'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
"$tmp/tail" >"$tmp/tail.out" 2>&1
if [ $? -ne 0 ] || ! grep -q '^RESULT: PASS' "$tmp/tail.out"; then
    echo 'MISS: the prepared run still made heap calls while the clock ran'
    grep '^RESULT:' "$tmp/tail.out" | head -1
    exit 1
fi
cat >"$tmp/probe.cpp" <<CPP
#define POOL_TAIL_NO_MAIN
#include "$source_path"
// The probe counts upstream requests with its OWN resource, not the lab's counters, so
// a counter edited in the learner's file cannot hide a refill.
struct probe_resource : std::pmr::memory_resource {
    std::size_t calls = 0;
    void* do_allocate(std::size_t n, std::size_t a) override { ++calls; return std::pmr::new_delete_resource()->allocate(n, a); }
    void do_deallocate(void* p, std::size_t n, std::size_t a) override { std::pmr::new_delete_resource()->deallocate(p, n, a); }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};
static bool tight(std::size_t peak) {
    probe_resource heap;
    fixed_pool pool(64, &heap);
    std::pmr::list<Order> book(&pool);
    prepare_for_peak(book, peak);
    if (!book.empty()) return false;
    const std::size_t before = heap.calls;
    for (std::size_t i = 0; i < peak; ++i) book.push_back(Order{i, 1.0, 1, 'B'});
    return heap.calls == before && heap.calls > 0;
}
int main() { return (tight(777) && tight(5000) && tight(1) && tight(kPeak + 10000)) ? 0 : 1; }
CPP
if ! clang++ -std=c++20 -O1 -I "$hdr" "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: prepare_for_peak does not compile against the probe'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
if ! "$tmp/probe"; then
    echo 'MISS: prepare_for_peak must leave the book empty and the pool holding blocks for any peak it is given'
    exit 1
fi
echo 'HIT (prepared run: zero heap calls while timed; prepare_for_peak works for other peaks)'
