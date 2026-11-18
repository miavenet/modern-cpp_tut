// clock_hist.cpp - per-call bracket histogram of clock reads on macOS / Apple Silicon.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_hist.cpp -o "${TMPDIR:-/tmp}/clock_hist" && "${TMPDIR:-/tmp}/clock_hist"
// For each clock: read it twice back to back, record the difference (in that
// clock's own units), 1,000,000 times per run, 5 runs. Print p50, p99.9,
// p99.99 and max of each run, plus the smallest non-zero step seen.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <mach/mach_time.h>
#include <vector>

template <class F>
static void run(const char* name, const char* unit, F read) {
  constexpr int N = 1'000'000;
  std::vector<uint64_t> d(N);
  for (int r = 0; r < 5; ++r) {
    for (int i = 0; i < N; ++i) {
      uint64_t a = read();
      uint64_t b = read();
      d[i] = b - a;
    }
    std::sort(d.begin(), d.end());
    uint64_t minstep = 0;
    for (auto x : d) if (x) { minstep = x; break; }
    std::printf("%-22s run%d  p50=%llu p99.9=%llu p99.99=%llu max=%llu  min_nonzero=%llu (%s)\n",
                name, r, (unsigned long long)d[N / 2], (unsigned long long)d[(size_t)N * 999 / 1000],
                (unsigned long long)d[(size_t)N * 9999 / 10000], (unsigned long long)d[N - 1],
                (unsigned long long)minstep, unit);
  }
}

int main() {
  mach_timebase_info_data_t tb;
  mach_timebase_info(&tb);
  uint64_t freq;
  asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
  std::printf("mach_timebase numer/denom = %u/%u ; cntfrq_el0 = %llu Hz\n", tb.numer, tb.denom,
              (unsigned long long)freq);
  run("mach_absolute_time", "ticks", [] { return mach_absolute_time(); });
  run("CLOCK_UPTIME_RAW", "ns", [] { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW); });
  run("chrono::steady_clock", "ns", [] {
    return (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
  });
}
