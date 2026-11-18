// pf_chase.cpp - dependent lookups: each index comes from the previous load (argument: table size log2).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pf_chase.cpp -o "${TMPDIR:-/tmp}/pf_chase" && for r in 1 2 3; do "${TMPDIR:-/tmp}/pf_chase" 28; done
// Dependent lookups: each index comes from the previous load, so loads cannot overlap.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const int lg = std::atoi(argv[1]);
  const size_t n = (size_t(1) << lg) / sizeof(uint64_t);
  const size_t lookups = size_t(1) << 23;
  std::vector<uint64_t> table(n);
  uint64_t x = 88172645463325252ull;
  for (auto& v : table) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; v = x; }
  auto t0 = std::chrono::steady_clock::now();
  uint64_t k = 1, sum = 0;
  for (size_t i = 0; i < lookups; ++i) { uint64_t v = table[k % n]; sum += v; k = v ^ i; }
  auto t1 = std::chrono::steady_clock::now();
  std::printf("chase table=2^%d B checksum=%llu ns_per_lookup=%.2f\n", lg, (unsigned long long)sum,
              std::chrono::duration<double, std::nano>(t1 - t0).count() / double(lookups));
}
