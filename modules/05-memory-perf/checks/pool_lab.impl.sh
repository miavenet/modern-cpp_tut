#!/bin/bash
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
source_path=$(cat)
case "$source_path" in /*) ;; *) source_path="$root/$source_path" ;; esac
if [ ! -f "$source_path" ]; then echo "MISS: workspace file not found: $source_path"; exit 1; fi
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || {
    echo 'SETUP PROBLEM: cannot create a temporary build directory'
    exit 2
}
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'
    exit 2
fi
if ! cp "$source_path" "$tmp/pool_lab.cpp"; then
    echo 'SETUP PROBLEM: cannot copy the lab source into the temporary build directory'
    exit 2
fi
if ! clang++ -std=c++20 -Wall -Wextra -O1 "$tmp/pool_lab.cpp" -o "$tmp/example" >"$tmp/build.log" 2>&1; then
    echo 'MISS: the pool lab does not compile'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
"$tmp/example" >"$tmp/example.out" 2>&1
if [ $? -ne 0 ] || ! grep -q '^RESULT: PASS' "$tmp/example.out"; then
    echo 'MISS: the pool lab did not pass its churn and upstream-count test'
    grep '^RESULT:' "$tmp/example.out" | head -1
    exit 1
fi
cat >"$tmp/probe.cpp" <<'CPP'
#define main pool_example_main
#include "pool_lab.cpp"
#undef main
#include <cstdlib>
#include <limits>

static bool fail_next_new = false;
void* operator new(std::size_t n) {
    if (fail_next_new) { fail_next_new = false; throw std::bad_alloc(); }
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

class malloc_resource : public std::pmr::memory_resource {
public:
    int allocations = 0, deallocations = 0;
private:
    void* do_allocate(std::size_t n, std::size_t) override {
        ++allocations;
        if (void* p = std::malloc(n)) return p;
        throw std::bad_alloc();
    }
    void do_deallocate(void* p, std::size_t, std::size_t) override {
        ++deallocations;
        std::free(p);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override {
        return this == &o;
    }
};
int main() {
    counting_resource upstream;
    fixed_pool pool(2, &upstream);
    void* z = pool.allocate(0, 8);
    if (pool.bound_size() != 0 || pool.forwarded() != 1) return 2;
    pool.deallocate(z, 0, 8);
    void* a = pool.allocate(32, 8);
    pool.deallocate(a, 32, 8);
    void* b = pool.allocate(32, 16);
    bool forwarded = pool.forwarded() == 2;
    pool.deallocate(b, 32, 16);
    if (!forwarded) return 3;
    try { fixed_pool bad(0, &upstream); return 4; }
    catch (const std::bad_alloc&) {}
    try { fixed_pool bad(2, nullptr); return 5; }
    catch (const std::bad_alloc&) {}
    fixed_pool huge(2, &upstream);
    try { huge.allocate(std::numeric_limits<std::size_t>::max(), 8); return 6; }
    catch (const std::bad_alloc&) {}
    if (huge.bound_size() != 0) return 7;
    malloc_resource bookkeeping_upstream;
    {
        fixed_pool failing(2, &bookkeeping_upstream);
        fail_next_new = true; // the chunk uses malloc; vector bookkeeping uses new
        try { failing.allocate(32, 8); return 8; }
        catch (const std::bad_alloc&) {}
        if (bookkeeping_upstream.allocations != 1 ||
            bookkeeping_upstream.deallocations != 1) return 9;
        void* retry = failing.allocate(32, 8);
        failing.deallocate(retry, 32, 8);
    }
    if (bookkeeping_upstream.allocations != bookkeeping_upstream.deallocations) return 10;
    return 0;
}
CPP
if ! clang++ -std=c++20 -O1 "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: the pool API does not compile against the alignment probe'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
if ! "$tmp/probe"; then
    echo 'MISS: zero-size, overflow, bookkeeping failure, or alignment-class probe failed'
    exit 1
fi
echo 'HIT (churn, upstream accounting, zero-size, overflow, bookkeeping failure, alignment class)'
