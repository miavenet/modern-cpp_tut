// pf_gather.cpp - random lookups into a table, with and without a software prefetch D lookups ahead (arguments: table size log2, distance; 0 = off).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pf_gather.cpp -o "${TMPDIR:-/tmp}/pf_gather" && for r in 1 2 3; do "${TMPDIR:-/tmp}/pf_gather" 18 0; "${TMPDIR:-/tmp}/pf_gather" 18 16; "${TMPDIR:-/tmp}/pf_gather" 28 16; "${TMPDIR:-/tmp}/pf_gather" 28 0; done
// Random lookups into a table, with and without a software prefetch D lookups ahead.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const int lg = std::atoi(argv[1]); const int D = std::atoi(argv[2]);
  const size_t n = (size_t(1) << lg) / sizeof(uint64_t);
  const size_t lookups = size_t(1) << 23;                 // 8M lookups
  std::vector<uint64_t> table(n);
  for (size_t i = 0; i < n; ++i) table[i] = i * 0x9E3779B97F4A7C15ull;
  std::vector<uint32_t> idx(lookups);
  uint64_t x = 88172645463325252ull;                      // xorshift64, fixed seed
  for (auto& k : idx) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; k = uint32_t(x % n); }
  auto t0 = std::chrono::steady_clock::now();
  uint64_t sum = 0;
  for (size_t i = 0; i < lookups; ++i) {
    if (D && i + D < lookups) __builtin_prefetch(&table[idx[i + D]], 0, 3);
    sum += table[idx[i]];
  }
  auto t1 = std::chrono::steady_clock::now();
  double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / double(lookups);
  std::printf("table=2^%d B D=%d checksum=%llu ns_per_lookup=%.2f\n", lg, D, (unsigned long long)sum, ns);
}
