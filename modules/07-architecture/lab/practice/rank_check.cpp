// rank_check.cpp - correctness check before timing: three ways to answer
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/rank_check.cpp -o "${TMPDIR:-/tmp}/rank_check" && "${TMPDIR:-/tmp}/rank_check"
// "how many values are strictly less than x?" over one sorted array, fed the same seeded queries.
//   Reference: a prefix-count table (a different algorithm, easy to trust).
//   Fast:      std::lower_bound.
//   Buggy:     std::upper_bound (counts values <= x; wrong when x is present).
// Each folds its answers into a checksum; a contestant must match the reference.
// Values and queries come from std::mt19937_64 raw output (no distributions),
// reduced with % 1000 so duplicates exist.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

constexpr int NV = 100'000, NQ = 100'000, RANGE = 1000;

static uint64_t fold(uint64_t h, uint64_t a) { return h * 1'000'003u + a; }

int main() {
  std::mt19937_64 g(42);
  std::vector<uint32_t> v(NV);
  for (auto& x : v) x = (uint32_t)(g() % RANGE);
  std::sort(v.begin(), v.end());
  std::vector<uint32_t> q(NQ);
  for (auto& x : q) x = (uint32_t)(g() % RANGE);

  std::vector<uint64_t> less(RANGE + 1, 0);  // less[x] = count of values < x
  for (auto x : v) ++less[x + 1];
  for (int i = 1; i <= RANGE; ++i) less[i] += less[i - 1];

  uint64_t ref = 0, fast = 0, buggy = 0;
  for (auto x : q) {
    ref = fold(ref, less[x]);
    fast = fold(fast, (uint64_t)(std::lower_bound(v.begin(), v.end(), x) - v.begin()));
    buggy = fold(buggy, (uint64_t)(std::upper_bound(v.begin(), v.end(), x) - v.begin()));
  }
  std::printf("reference  %016llx\n", (unsigned long long)ref);
  std::printf("fast       %016llx %s\n", (unsigned long long)fast, fast == ref ? "PASS" : "FAIL");
  std::printf("buggy      %016llx %s\n", (unsigned long long)buggy, buggy == ref ? "PASS" : "FAIL");
}
