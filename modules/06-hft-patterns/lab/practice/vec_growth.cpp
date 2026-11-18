// vec_growth.cpp - where does the cost of a growing std::vector land? Time every push_back.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/vec_growth.cpp -o "${TMPDIR:-/tmp}/vec_growth" && "${TMPDIR:-/tmp}/vec_growth" grow && "${TMPDIR:-/tmp}/vec_growth" reserve && "${TMPDIR:-/tmp}/vec_growth" prefault
// Modes (run each in its own process so fault counts are clean):
//   grow     : no reserve
//   reserve  : reserve(N) before the timed loop
//   prefault : reserve(N), then resize(N) + clear() before the timed loop (pages touched at startup)
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

struct Rec { uint64_t id, px, qty, ts, flags, pad; };  // 48 bytes
static_assert(sizeof(Rec) == 48);

static long minflt() { rusage u; getrusage(RUSAGE_SELF, &u); return u.ru_minflt; }

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "grow";
  const size_t N = 1000000;
  std::vector<Rec> v;
  if (!strcmp(mode, "reserve") || !strcmp(mode, "prefault")) v.reserve(N);
  if (!strcmp(mode, "prefault")) { v.resize(N); v.clear(); }
  std::vector<int64_t> t(N);           // per-call ns (value-initialised, so its pages are touched before timing)
  std::vector<char> grew(N, 0);        // did this call change capacity?
  size_t reallocs = 0;
  long f0 = minflt();
  for (size_t i = 0; i < N; ++i) {
    size_t cap = v.capacity();
    auto a = std::chrono::steady_clock::now();
    v.push_back(Rec{i, i * 3, 1, i, 0, 0});
    auto b = std::chrono::steady_clock::now();
    t[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    if (v.capacity() != cap) { grew[i] = 1; ++reallocs; }
  }
  long f1 = minflt();
  std::vector<size_t> idx(N);
  for (size_t i = 0; i < N; ++i) idx[i] = i;
  std::sort(idx.begin(), idx.end(), [&](size_t x, size_t y) { return t[x] > t[y]; });
  int top_grew = 0;
  for (int k = 0; k < 10; ++k) top_grew += grew[idx[k]];
  std::vector<int64_t> s = t; std::sort(s.begin(), s.end());
  int64_t sum = 0; for (auto x : t) sum += x;
  printf("mode=%-8s page=%ld sizeof(Rec)=%zu N=%zu | capacity changes in loop=%zu | minor faults in loop=%ld | "
         "p50=%lld p99=%lld p99.9=%lld max=%lld ns | sum=%.2f ms | of the 10 slowest calls, %d changed capacity\n",
         mode, sysconf(_SC_PAGESIZE), sizeof(Rec), N, reallocs, f1 - f0,
         (long long)s[N / 2], (long long)s[N * 99 / 100], (long long)s[N * 999 / 1000], (long long)s[N - 1],
         sum / 1e6, top_grew);
  return (int)(v.size() != N);
}
