#!/bin/bash
# Check for M5-LPA5: the upstream that makes a prepared pool fail LOUDLY past
# its planned peak instead of quietly calling the heap. Learner's answer on
# stdin: one C++ expression for the blank in
#   std::pmr::monotonic_buffer_resource slab(buf, sizeof buf, ___);
# Graded on behaviour, never on time: past the slab the program must throw
# std::bad_alloc (a heap upstream would quietly succeed instead).
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*;$//')
if [ -z "$answer" ]; then echo 'MISS: give one C++ expression for the blank'; exit 1; fi
case "$answer" in
    *';'*|*'{'*|*'}'*|*'#'*|*'\'*) echo 'MISS: give one expression, not statements'; exit 1 ;;
    nullptr|NULL|0) echo 'MISS: a null pointer is not a memory resource; past the slab that is undefined behaviour'; exit 1 ;;
esac
# A heap-allocated resource (`new ...`) would leak and quietly reach the heap upstream.
if printf '%s' "$answer" | grep -Eq '(^|[^A-Za-z0-9_])new([^A-Za-z0-9_]|$)'; then
    echo 'MISS: do not allocate a resource with new: it leaks, and it is the heap you were told to keep out. Name an existing resource'; exit 1
fi
case "$answer" in
    *make_unique*|*make_shared*|*malloc*|*calloc*|*allocate*|*default_resource*) echo 'MISS: name an existing resource that refuses to allocate; do not build one on the heap or change the default resource'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'; exit 2
fi
pool="$root/modules/05-memory-perf/lab/solution/pool_lab.cpp"
if [ ! -f "$pool" ]; then echo 'SETUP PROBLEM: the reference pool_lab.cpp is missing'; exit 2; fi
cat >"$tmp/probe.cpp" <<CPP
#define main pool_example_main
#include "$pool"
#undef main
alignas(64) static std::byte buf[64 * 1024];   // room for a few 256-block chunks
int main() {
    using namespace std::pmr;
    std::pmr::monotonic_buffer_resource slab(buf, sizeof buf, $answer);
    fixed_pool pool(256, &slab);
    std::pmr::list<Order> book(&pool);
    bool threw = false;
    try {
        for (std::uint64_t i = 0; i < 100000; ++i) {
            book.push_back(Order{i, 1.0, 1, 'B'});
        }
    } catch (const std::bad_alloc&) { threw = true; }
    if (!threw) return 3;                          // overflow went somewhere quietly
    if (std::pmr::get_default_resource() != std::pmr::new_delete_resource()) return 5;
    return book.size() < 100000 ? 0 : 4;
}
CPP
if ! clang++ -std=c++20 -O1 "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that expression does not compile as the upstream of a monotonic_buffer_resource'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
"$tmp/probe" >"$tmp/run.log" 2>&1
status=$?
case $status in
    0) echo 'HIT (past the slab the pool threw std::bad_alloc instead of calling the heap)'; exit 0 ;;
    3) echo 'MISS: past the slab the allocation still succeeded, so a refill quietly reached another resource'; exit 1 ;;
    *) echo "MISS: the program crashed or misbehaved past the slab (exit $status); a null pointer is not a memory resource"; exit 1 ;;
esac
