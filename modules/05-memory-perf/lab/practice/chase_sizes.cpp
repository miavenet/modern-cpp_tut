// chase_sizes.cpp - a dependent-load chase through a random cycle of 128-byte-spaced slots, 16 KiB to 256 MiB.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/chase_sizes.cpp -o "${TMPDIR:-/tmp}/chase_sizes" && for i in 1 2 3; do "${TMPDIR:-/tmp}/chase_sizes"; done
// Dependent-load chase through a random cycle of 128-byte-spaced slots.
// Prints ns per load for working sets from 16 KiB to 256 MiB.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>
constexpr std::size_t STRIDE = 128 / sizeof(std::uint32_t);  // one index per 128 B
int main() {
  std::mt19937_64 rng(12345);
  for (std::size_t kib = 16; kib <= 256 * 1024; kib *= 2) {
    std::size_t slots = kib * 1024 / 128;
    std::vector<std::uint32_t> mem(slots * STRIDE);
    std::vector<std::uint32_t> order(slots);
    std::iota(order.begin(), order.end(), 0u);
    std::shuffle(order.begin() + 1, order.end(), rng);
    for (std::size_t i = 0; i < slots; ++i)
      mem[order[i] * STRIDE] = order[(i + 1) % slots] * STRIDE;
    const long loads = 20'000'000;
    std::uint32_t p = 0;
    for (long i = 0; i < loads / 10; ++i) p = mem[p];        // warm up
    auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < loads; ++i) p = mem[p];
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / loads;
    std::printf("%8zu KiB  %6.2f ns/load  (p=%u)\n", kib, ns, p);
  }
}
