// tick_step.cpp - the smallest step the clock shows, next to its declared period, clock_getres and the counter frequency.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/tick_step.cpp -o "${TMPDIR:-/tmp}/tick_step" && "${TMPDIR:-/tmp}/tick_step"
// What is the smallest step each clock can show?
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <time.h>
#if defined(__aarch64__)
static inline std::uint64_t cntfrq() { std::uint64_t v; asm volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v; }
#endif
int main() {
  using clk = std::chrono::steady_clock;
  std::printf("steady_clock period = %lld/%lld s\n", (long long)clk::period::num, (long long)clk::period::den);
  std::map<long long, int> seen;  // nonzero deltas between back-to-back reads
  int zeros = 0;
  for (int i = 0; i < 200000; ++i) {
    auto a = clk::now(), b = clk::now();
    long long d = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    if (d == 0) ++zeros; else ++seen[d];
  }
  std::printf("steady_clock: zero deltas %d of 200000; smallest nonzero deltas (ns:count):", zeros);
  int k = 0; for (auto& [d, c] : seen) { if (k++ == 4) break; std::printf(" %lld:%d", d, c); }
  std::printf("\n");
  { timespec r{}; int rc = clock_getres(CLOCK_MONOTONIC, &r); std::printf("clock_getres(CLOCK_MONOTONIC) rc=%d -> %lld s %ld ns\n", rc, (long long)r.tv_sec, (long)r.tv_nsec); }
#if defined(__aarch64__)
  std::uint64_t f = cntfrq();
  std::printf("cntfrq_el0 = %llu Hz -> one tick = %.2f ns\n", (unsigned long long)f, 1e9 / double(f));
#endif
}
