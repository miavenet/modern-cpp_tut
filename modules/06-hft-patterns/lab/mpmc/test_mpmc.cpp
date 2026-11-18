// test_mpmc.cpp -- 4 producers and 4 consumers move 400,000 values through MpmcRing.
// Run from the tutorial root:
//   clang++ -std=c++20 -O1 -g -fsanitize=thread modules/06-hft-patterns/lab/mpmc/test_mpmc.cpp \
//     -o "${TMPDIR:-/tmp}/mpmc" && "${TMPDIR:-/tmp}/mpmc"
#include "mpmc_ring.hpp"
#include <cstdio>
#include <thread>
#include <vector>

int main() {
    MpmcRing<int, 2> small;
    int first = 0, second = 0;
    const bool capacity_ok = small.try_push(11) && small.try_push(22) &&
        !small.try_push(33) && small.try_pop(first) && first == 11 &&
        small.try_pop(second) && second == 22 && !small.try_pop(first);
    if (!capacity_ok) {
        std::puts("capacity-two full/empty: FAIL");
        return 1;
    }
    std::puts("capacity-two full/empty: PASS");
    constexpr int kProducers = 4, kConsumers = 4, kPer = 100'000;
    static MpmcRing<long, 1024> q;
    std::atomic<long> sum{0}, received{0};
    std::vector<std::thread> threads;
    for (int p = 0; p < kProducers; ++p)
        threads.emplace_back([p] { for (long i = 0; i < kPer;) if (q.try_push(p * kPer + i)) ++i; });
    for (int c = 0; c < kConsumers; ++c)
        threads.emplace_back([&] {
            long v;
            while (received.load() < kProducers * kPer)
                if (q.try_pop(v)) { sum += v; ++received; }
        });
    for (auto& t : threads) t.join();
    long n = long(kProducers) * kPer;
    bool ok = received == n && sum == n * (n - 1) / 2;
    std::printf("received %ld values, sum %s\nRESULT: %s\n", received.load(), ok ? "correct" : "WRONG", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
