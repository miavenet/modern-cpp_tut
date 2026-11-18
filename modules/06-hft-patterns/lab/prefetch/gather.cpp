// gather.cpp -- does software prefetch help a random gather on YOUR machine?
// Build and run (from the tutorial root; takes about 3 seconds, uses about 450 MiB):
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/prefetch/gather.cpp -o "${TMPDIR:-/tmp}/gather"
//   "${TMPDIR:-/tmp}/gather"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

constexpr std::size_t kTable = std::size_t(1) << 25;   // 32 Mi entries x 8 bytes = 256 MiB
constexpr std::size_t kLookups = std::size_t(1) << 24; // 16 Mi random lookups

// Sum table[idx[i]] for every i. Distance d > 0 prefetches the entry needed
// d iterations from now; d == 0 means no prefetch at all.
[[gnu::noinline]]   // keep the timed work inside the timed region (see module 07 lesson 03)
std::uint64_t gather(const std::vector<std::uint64_t>& table,
                     const std::vector<std::uint32_t>& idx, std::size_t d) {
    std::uint64_t sum = 0;
    const std::size_t n = idx.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (d != 0 && i + d < n)                        // stay inside idx
            __builtin_prefetch(&table[idx[i + d]], 0, 0);
        sum += table[idx[i]];
    }
    return sum;
}

// Sequential sum, optionally prefetching 64 elements ahead (d == 0: none).
[[gnu::noinline]]
std::uint64_t scan(const std::vector<std::uint64_t>& table, std::size_t d) {
    std::uint64_t sum = 0;
    const std::size_t n = table.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (d != 0 && i + d < n) __builtin_prefetch(&table[i + d], 0, 0);
        sum += table[i];
    }
    return sum;
}

// Pointer chase: each step's address is the value loaded by the previous step,
// so the loads cannot overlap and no future address is known in advance.
[[gnu::noinline]]
std::uint64_t chase(const std::vector<std::uint32_t>& next, std::size_t steps) {
    std::uint32_t at = 0;
    for (std::size_t i = 0; i < steps; ++i) at = next[at];
    return at;
}

int main() {
    std::vector<std::uint64_t> table(kTable);
    std::iota(table.begin(), table.end(), 0);
    std::vector<std::uint32_t> idx(kLookups);
    std::mt19937 rng(42);
    for (auto& x : idx) x = static_cast<std::uint32_t>(rng() % kTable);

    for (std::size_t d : {0, 1, 4, 16, 64, 256}) {
        double best = 1e30;
        std::uint64_t sum = 0;
        for (int rep = 0; rep < 3; ++rep) {             // best of 3 runs
            auto t0 = std::chrono::steady_clock::now();
            sum = gather(table, idx, d);
            auto t1 = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / kLookups);
        }
        std::printf("gather, distance %3zu: %6.2f ns per lookup (sum %llu)\n", d, best, (unsigned long long)sum);
    }

    for (std::size_t d : {0, 64}) {
        auto t0 = std::chrono::steady_clock::now();
        auto sum = scan(table, d);
        auto t1 = std::chrono::steady_clock::now();
        std::printf("sequential, distance %2zu: %5.2f ns per element (sum %llu)\n", d,
                    std::chrono::duration<double, std::nano>(t1 - t0).count() / kTable,
                    (unsigned long long)sum);
    }

    // One random cycle through all entries (Sattolo's shuffle), then chase it.
    std::vector<std::uint32_t> next(kTable);
    std::iota(next.begin(), next.end(), 0u);
    for (std::size_t i = kTable - 1; i > 0; --i) std::swap(next[i], next[rng() % i]);
    constexpr std::size_t kSteps = std::size_t(1) << 20;
    auto t0 = std::chrono::steady_clock::now();
    auto end = chase(next, kSteps);
    auto t1 = std::chrono::steady_clock::now();
    std::printf("pointer chase:          %6.2f ns per step (end %llu)\n",
                std::chrono::duration<double, std::nano>(t1 - t0).count() / kSteps, (unsigned long long)end);
}
