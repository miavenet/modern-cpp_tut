// telemetry.cpp - distinct cache lines touched by a scan of one hot field: one record vs a hot/cold split.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/telemetry.cpp -o "${TMPDIR:-/tmp}/telemetry" && "${TMPDIR:-/tmp}/telemetry"
// Count the distinct cache lines a scan of one hot field touches, for an
// all-in-one record versus a hot/cold split. Exact counts from addresses, no timing.
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>
struct Reading {                 // everything together
  float temp;                    // hot: scanned for every sample
  std::uint32_t seq;             // hot
  char device_id[24];            // cold below
  char site_name[64];
  double calib[16];
};
struct ReadingHot { float temp; std::uint32_t seq; };
template <class T> std::size_t lines_touched(const std::vector<T>& v, std::size_t line) {
  std::set<std::uintptr_t> s;
  for (const auto& x : v) {
    auto a = reinterpret_cast<std::uintptr_t>(&x.temp);
    s.insert(a / line); s.insert((a + sizeof(float) - 1) / line);
  }
  return s.size();
}
int main() {
  constexpr std::size_t n = 6000;
  std::vector<Reading> all(n);
  std::vector<ReadingHot> hot(n);
  std::printf("sizeof(Reading)=%zu sizeof(ReadingHot)=%zu\n", sizeof(Reading), sizeof(ReadingHot));
  std::printf("vector data address mod 128: all=%zu hot=%zu\n",
              (std::size_t)(reinterpret_cast<std::uintptr_t>(all.data()) % 128),
              (std::size_t)(reinterpret_cast<std::uintptr_t>(hot.data()) % 128));
  for (std::size_t line : {64, 128})
    std::printf("line=%3zu B: lines touched scanning temp of %zu records: together=%zu split=%zu\n",
                line, n, lines_touched(all, line), lines_touched(hot, line));
}
