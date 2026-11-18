// clock_res.cpp - declared unit vs real resolution of several clocks, and what
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/clock_res.cpp -o "${TMPDIR:-/tmp}/clock_res" && "${TMPDIR:-/tmp}/clock_res"
// that does to single-shot timing of a short operation.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <map>
#include <type_traits>
#include <mach/mach_time.h>
#include <vector>

template <class Clock>
void probe(const char* name) {
  const int R = 1000000;
  std::vector<int64_t> d(R);
  auto w0 = std::chrono::steady_clock::now();
  auto prev = Clock::now();
  for (int i = 0; i < R; ++i) {
    auto t = Clock::now();
    d[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t - prev).count();
    prev = t;
  }
  double wall = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - w0).count();
  int64_t zeros = 0, minnz = INT64_MAX;
  for (auto x : d) { if (x == 0) ++zeros; else if (x > 0 && x < minnz) minnz = x; }
  using P = typename Clock::period;
  printf("%-22s declared period %lld/%lld s | min non-zero step %lld ns | zero deltas %.1f%% | wall/read %.1f ns\n",
         name, (long long)P::num, (long long)P::den, (long long)minnz, 100.0 * zeros / R, wall / R);
}

struct ThreadCpu {  // CLOCK_THREAD_CPUTIME_ID wrapped as a chrono-like clock
  using period = std::nano;
  using duration = std::chrono::nanoseconds;
  struct time_point { int64_t ns; duration operator-(time_point o) const { return duration(ns - o.ns); } };
  static time_point now() { timespec ts; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return {ts.tv_sec * 1000000000LL + ts.tv_nsec}; }
};

static uint64_t work(uint64_t x) {  // a short dependent chain (multiply + xor-shift, 8 steps); not foldable
  for (int i = 0; i < 8; ++i) x = (x * 0x9E3779B97F4A7C15ULL) ^ (x >> 29);
  return x;
}

int main() {
  mach_timebase_info_data_t tb; mach_timebase_info(&tb);
  printf("mach_timebase_info numer/denom = %u/%u (ns per raw tick = %.2f)\n", tb.numer, tb.denom, (double)tb.numer / tb.denom);
  printf("high_resolution_clock is steady_clock: %d\n", (int)std::is_same_v<std::chrono::high_resolution_clock, std::chrono::steady_clock>);
  probe<std::chrono::steady_clock>("steady_clock");
  probe<std::chrono::system_clock>("system_clock");
  probe<std::chrono::high_resolution_clock>("high_resolution_clock");
  probe<ThreadCpu>("CLOCK_THREAD_CPUTIME_ID");

  // Single-shot timing of work() vs one batched measurement.
  const int S = 200000;
  std::map<int64_t, int> hist;
  volatile uint64_t sink = 1;
  for (int i = 0; i < S; ++i) {
    auto a = std::chrono::steady_clock::now();
    sink = work(sink);
    auto b = std::chrono::steady_clock::now();
    ++hist[std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count()];
  }
  printf("single-shot work(): %d samples; most common values (ns:count):", S);
  int shown = 0;
  for (auto& [v, c] : hist) if (c * 1000 >= S && shown < 8) { printf(" %lld:%d", (long long)v, c); ++shown; }
  printf("\n");
  const int B = 20000000;
  uint64_t x = sink;
  auto a = std::chrono::steady_clock::now();
  for (int i = 0; i < B; ++i) x = work(x);
  auto b = std::chrono::steady_clock::now();
  sink = x;
  printf("batched work(): %.2f ns per call over %d calls\n",
         std::chrono::duration<double, std::nano>(b - a).count() / B, B);
}
