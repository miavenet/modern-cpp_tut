// filter.cpp - branchy filter over a price array: keep the index of every price above a limit.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/filter.cpp -o "${TMPDIR:-/tmp}/filter" && "${TMPDIR:-/tmp}/filter" 0.8 1000 && "${TMPDIR:-/tmp}/filter" 0.8 0
// Usage: filter <pass_rate> <period>   (period 0 = non-repeating)
// Input A repeats a short block (period P); input B never repeats. Same target pass rate.
// Listing: clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/filter.cpp -o -   (look at branchy)
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
static uint64_t rng = 0x243F6A8885A308D3ull;  // xorshift64*, self-written
static inline uint64_t nxt() { rng ^= rng >> 12; rng ^= rng << 25; rng ^= rng >> 27; return rng * 2685821657736338717ull; }
__attribute__((noinline)) size_t branchy(const uint32_t* px, size_t n, uint32_t limit, uint32_t* out) {
  size_t m = 0;
  for (size_t i = 0; i < n; ++i)
    if (px[i] > limit) out[m++] = (uint32_t)i;   // conditional store: compiled as a branch (check the .s)
  return m;
}
__attribute__((noinline)) size_t branchless(const uint32_t* px, size_t n, uint32_t limit, uint32_t* out) {
  size_t m = 0;
  for (size_t i = 0; i < n; ++i) { out[m] = (uint32_t)i; m += px[i] > limit; }  // control: no branch on the data
  return m;
}
int main(int argc, char** argv) {
  double p = argc > 1 ? atof(argv[1]) : 0.8;
  size_t period = argc > 2 ? strtoull(argv[2], 0, 10) : 0;
  const size_t n = 1u << 24; const uint32_t limit = 1000000;
  std::vector<uint32_t> px(n), out(n + 1);
  for (size_t i = 0; i < n; ++i)
    px[i] = (period && i >= period) ? px[i % period]
          : (uint32_t)((nxt() % 1000000) < p * 1000000 ? limit + 1 + nxt() % 1000000 : nxt() % (limit + 1));
  size_t pass = 0; for (auto x : px) pass += x > limit;
  printf("target_pass=%.3f realized_pass=%.4f period=%zu n=%zu\n", p, (double)pass / n, period, n);
  struct V { const char* name; size_t (*f)(const uint32_t*, size_t, uint32_t, uint32_t*); } vs[] = {{"branchy", branchy}, {"branchless", branchless}};
  for (auto& v : vs) {
    double t[5]; size_t r = 0;
    for (int k = 0; k < 5; ++k) {
      auto a = std::chrono::steady_clock::now(); r = v.f(px.data(), n, limit, out.data()); auto b = std::chrono::steady_clock::now();
      t[k] = std::chrono::duration<double, std::nano>(b - a).count() / n;
    }
    std::sort(t, t + 5);
    printf("  %-10s median %.3f ns/elem (min %.3f max %.3f) kept=%zu %s\n", v.name, t[2], t[0], t[4], r, r == pass ? "OK" : "MISMATCH");
  }
}
