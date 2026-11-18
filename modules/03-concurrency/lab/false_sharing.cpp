// false_sharing.cpp - two threads, two counters: same cache line or not?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/03-concurrency/lab/false_sharing.cpp -o "${TMPDIR:-/tmp}/false_sharing" && "${TMPDIR:-/tmp}/false_sharing"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

struct Packed { std::atomic<long> a{0}; std::atomic<long> b{0}; };
struct Padded { alignas(128) std::atomic<long> a{0}; alignas(128) std::atomic<long> b{0}; };

template <class Counters>
long long run_ms() {
    Counters c;
    auto t0 = std::chrono::steady_clock::now();
    std::thread t1([&] { for (int i = 0; i < 20'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
    std::thread t2([&] { for (int i = 0; i < 20'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    t1.join(); t2.join();
    auto t = std::chrono::steady_clock::now() - t0;
    return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

int main() {
    std::printf("sizeof(Packed)=%zu sizeof(Padded)=%zu\n", sizeof(Packed), sizeof(Padded));
    std::printf("packed: %lld ms\n", run_ms<Packed>());
    std::printf("padded: %lld ms\n", run_ms<Padded>());
}
