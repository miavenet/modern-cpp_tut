// pctl_log.cpp - how many slow requests can a percentile hide?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pctl_log.cpp -o "${TMPDIR:-/tmp}/pctl_log" && "${TMPDIR:-/tmp}/pctl_log"
// A request log holds N = 50,000 latencies: fast requests take 1 ms, k of them
// (cache misses) take 9 ms. Print p99, p99.9 and max for several k.
// Percentile definition: nearest-rank. The q-th percentile is the sample at
// 1-based rank ceil(q*N) in ascending order, i.e. 0-based index ceil(q*N)-1.
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

// nearest-rank 0-based index for q = num/den, integer arithmetic only
static size_t nr_index(size_t n, size_t num, size_t den) { return (n * num + den - 1) / den - 1; }

int main() {
  constexpr size_t N = 50'000;
  const size_t i99 = nr_index(N, 99, 100), i999 = nr_index(N, 999, 1000);
  std::printf("N=%zu  p99 index=%zu (samples above: %zu)  p99.9 index=%zu (samples above: %zu)\n", N, i99,
              N - 1 - i99, i999, N - 1 - i999);
  for (int k : {0, 1, 49, 50, 51, 499, 500, 501}) {
    std::vector<int> v(N, 1);
    std::mt19937 g(11);
    std::vector<size_t> idx(N);
    for (size_t i = 0; i < N; ++i) idx[i] = i;
    std::shuffle(idx.begin(), idx.end(), g);
    for (int i = 0; i < k; ++i) v[idx[i]] = 9;
    std::sort(v.begin(), v.end());
    std::printf("k=%4d  p99=%d ms  p99.9=%d ms  max=%d ms\n", k, v[i99], v[i999], v[N - 1]);
  }
}
