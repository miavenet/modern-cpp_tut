// test_spsc.cpp -- tests for the module 06 SPSC lab.
// Run from the tutorial root. Starter (fails under ThreadSanitizer):
//   clang++ -std=c++20 -O1 -g -fsanitize=thread -I modules/06-hft-patterns/lab/spsc/starter \
//     modules/06-hft-patterns/lab/spsc/test_spsc.cpp -o "${TMPDIR:-/tmp}/spsc" && "${TMPDIR:-/tmp}/spsc"
// Solution: the same command with .../lab/spsc/solution in place of .../starter.
#include "spsc_ring.hpp"
#include <cstdio>
#include <thread>

static int failures = 0;
static void expect(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

int main() {
    // 1. Single thread: empty, full, wrap-around.
    SpscRing<int, 4> r;
    int x = -1;
    expect(!r.try_pop(x), "pop on an empty ring returns false");
    for (int i = 0; i < 4; ++i) r.try_push(i);
    expect(!r.try_push(99), "push on a full ring (4 of 4) returns false");
    bool order_ok = true;
    for (int round = 0; round < 3; ++round)          // 12 values through 4 slots
        for (int i = 0; i < 4; ++i) {
            order_ok &= r.try_pop(x) && x == round * 4 + i;
            r.try_push((round + 1) * 4 + i);
        }
    expect(order_ok, "values come out in FIFO order across wrap-around");

    // 2. Two threads: one producer, one consumer, 1,000,000 values.
    constexpr int kCount = 1'000'000;
    static SpscRing<long, 1024> q;
    long sum = 0;
    bool in_order = true;
    std::thread consumer([&] {
        long expected = 0, v = 0;
        while (expected < kCount) {
            if (q.try_pop(v)) { in_order &= (v == expected); sum += v; ++expected; }
        }
    });
    for (long i = 0; i < kCount;) if (q.try_push(i)) ++i;
    consumer.join();
    expect(in_order, "consumer saw 0, 1, 2, ... in order");
    expect(sum == long(kCount) * (kCount - 1) / 2, "sum of received values is correct");

    std::printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
