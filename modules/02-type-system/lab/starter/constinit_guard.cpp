// Module 02 LPA lab, STARTER. Lesson 04 (concept C8, cards M2-LPA3 and M2-LPA4).
//
// 1. Copy it to your work folder (this OVERWRITES an earlier work copy):
//   mkdir -p modules/02-type-system/lab/work && cp modules/02-type-system/lab/starter/constinit_guard.cpp modules/02-type-system/lab/work/
// 2. Build and run from the tutorial root:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/02-type-system/lab/work/constinit_guard.cpp -o "${TMPDIR:-/tmp}/constinit_guard" && "${TMPDIR:-/tmp}/constinit_guard"
// 3. Count the hidden one-time-initialisation guard calls in the machine code (no timing):
//   clang++ -std=c++20 -O2 -S -o - modules/02-type-system/lab/work/constinit_guard.cpp | grep -c __cxa_guard_acquire
//
// Every thread numbers its own orders 1, 2, 3, ... from a per-thread counter that must
// start at 0 with no run-time work. The starter is wrong twice: the counter is one
// function-local static (shared by all threads, with a guard checked on every call and
// a lock taken on the first), and its starting value comes from a run-time function.
//
// Keep the names g_init_calls, start_value and next_id: the check calls them.

#include <cstdint>
#include <cstdio>
#include <thread>

std::uint64_t g_init_calls = 0;                         // counts run-time initialisation work
std::uint64_t start_value() { ++g_init_calls; return 0; }  // run-time code: not constexpr

// TODO(M2-LPA4): delete the function-local static and declare, at namespace scope above
// this function, ONE variable named seq: a std::uint64_t, one copy per thread, starting
// at 0, whose initialisation the compiler must prove is constant (and refuses to build
// otherwise). Then next_id() needs no other change.
std::uint64_t next_id() {
    static std::uint64_t seq = start_value();
    return ++seq;
}

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
