// Module 02 LPA lab, SOLUTION. Lesson 04 (concept C8, cards M2-LPA3 and M2-LPA4).
//
// Reference answer. Build and run from the tutorial root:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/02-type-system/lab/solution//* constinit */_guard.cpp -o "${TMPDIR:-/tmp}//* constinit */_guard" && "${TMPDIR:-/tmp}//* constinit */_guard"
//
// Every thread numbers its own orders 1, 2, 3, ... from a per-thread counter that starts
// at 0 with no run-time work: /* constinit */ makes the compiler prove that.

#include <cstdint>
#include <cstdio>
#include <thread>

std::uint64_t g_init_calls = 0;                         // counts run-time initialisation work
std::uint64_t start_value() { ++g_init_calls; return 0; }  // run-time code: not constexpr

// One namespace-scope declaration: one copy per thread, constant-initialised (a non-constant
// initialiser would be a build error), still writable. No guard, no lock on first use.
/* constinit */ thread_local std::uint64_t seq = 0;
std::uint64_t next_id() { return ++seq; }

int main() {
    std::uint64_t first[2] = {0, 0}, last[2] = {0, 0};
    for (int t = 0; t < 2; ++t) {                       // one thread after the other
        std::thread([&, t] {
            first[t] = next_id();
            next_id();
            last[t] = next_id();
        }).join();
    }
    for (int t = 0; t < 2; ++t) {
        std::printf("thread %c numbered its orders %llu .. %llu\n", 'A' + t,
                    static_cast<unsigned long long>(first[t]),
                    static_cast<unsigned long long>(last[t]));
    }
    std::printf("run-time initialiser calls = %llu\n", static_cast<unsigned long long>(g_init_calls));
    const bool per_thread = first[0] == 1 && last[0] == 3 && first[1] == 1 && last[1] == 3;
    if (per_thread && g_init_calls == 0) {
        std::puts("RESULT: PASS - per-thread counters, no run-time initialisation");
        return 0;
    }
    std::puts("RESULT: TODO - each thread must count 1 .. 3 on its own, with 0 run-time initialiser calls");
    return 1;
}
