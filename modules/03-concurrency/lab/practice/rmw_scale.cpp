// rmw_scale.cpp - one shared atomic counter vs private counters vs plain loads, as the thread count grows.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/rmw_scale.cpp -o "${TMPDIR:-/tmp}/rmw_scale" && for m in shared private load; do for t in 1 2 4 8; do "${TMPDIR:-/tmp}/rmw_scale" $m $t; done; done
//   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/rmw_scale.cpp -o -   (find each worker loop)
// Per-operation cost of one shared atomic counter vs private counters vs plain loads,
// as the number of threads grows. No writer of shared data except the RMW mode.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
struct alignas(128) Padded { std::atomic<unsigned long> v{0}; };
std::atomic<unsigned long> g_shared{0};
Padded g_priv[16];
std::atomic<bool> g_go{false};
int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "shared";
    int T = argc > 2 ? std::atoi(argv[2]) : 1;
    const long N = 5'000'000;
    std::vector<std::thread> th;
    std::atomic<unsigned long> sink{0};
    for (int t = 0; t < T; ++t)
        th.emplace_back([&, t] {
            while (!g_go.load(std::memory_order_acquire)) {}
            unsigned long s = 0;
            if (!std::strcmp(mode, "shared"))
                for (long i = 0; i < N; ++i) g_shared.fetch_add(1, std::memory_order_relaxed);
            else if (!std::strcmp(mode, "private"))
                for (long i = 0; i < N; ++i) g_priv[t].v.fetch_add(1, std::memory_order_relaxed);
            else
                for (long i = 0; i < N; ++i) s += g_shared.load(std::memory_order_acquire);
            sink.fetch_add(s, std::memory_order_relaxed);
        });
    auto t0 = std::chrono::steady_clock::now();
    g_go.store(true, std::memory_order_release);
    for (auto& x : th) x.join();
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    // wall time per operation per thread (each thread did N ops in parallel)
    std::printf("mode=%s threads=%d wall_ns_per_op_per_thread=%.2f check=%lu\n",
                mode, T, ns / N, g_shared.load() + sink.load() % 1);
}
