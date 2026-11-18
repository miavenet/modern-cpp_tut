// hashq.cpp - hash quality: same keys (nanosecond timestamps), two hash functions.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/hashq.cpp -o "${TMPDIR:-/tmp}/hashq" && "${TMPDIR:-/tmp}/hashq"
// Print the bucket-size counts first, then insert/lookup times over 7 reps.
// Key stream and probe order use only raw std::mt19937_64 output (portable), with
// our own Fisher-Yates shuffle (std::shuffle's algorithm is not specified).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

struct MsHash {  // the bug under test: "hash the timestamp at millisecond granularity"
  size_t operator()(uint64_t ts) const { return ts / 1000000; }
};

template <class H>
void run(const char* name, const std::vector<uint64_t>& keys, const std::vector<uint64_t>& probe) {
  const int R = 7;
  std::vector<double> ins, look;
  size_t buckets = 0, nonempty = 0, maxb = 0;
  uint64_t chk = 0;
  for (int r = 0; r < R; ++r) {
    std::unordered_map<uint64_t, uint64_t, H> m;
    auto a = std::chrono::steady_clock::now();
    for (auto k : keys) m.emplace(k, k);
    auto b = std::chrono::steady_clock::now();
    for (auto k : probe) chk += m.find(k)->second;
    auto c = std::chrono::steady_clock::now();
    ins.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    look.push_back(std::chrono::duration<double, std::milli>(c - b).count());
    buckets = m.bucket_count(); nonempty = 0; maxb = 0;
    for (size_t i = 0; i < buckets; ++i) { size_t s = m.bucket_size(i); nonempty += s != 0; maxb = std::max(maxb, s); }
  }
  std::sort(ins.begin(), ins.end()); std::sort(look.begin(), look.end());
  printf("%-10s keys=%zu buckets=%zu non_empty=%zu max_bucket=%zu | insert ms min %.1f med %.1f max %.1f | lookup ms min %.1f med %.1f max %.1f (chk %llu)\n",
         name, keys.size(), buckets, nonempty, maxb, ins[0], ins[R / 2], ins[R - 1], look[0], look[R / 2], look[R - 1],
         (unsigned long long)(chk & 0xff));
}

int main() {
  const size_t N = 1 << 19;
  std::mt19937_64 g(7);
  std::vector<uint64_t> keys(N);
  const uint64_t base = 1700000000000000000ULL;            // some epoch time in ns
  for (size_t i = 0; i < N; ++i) keys[i] = base + i * 1500 + g() % 1000;  // strictly increasing, distinct
  std::vector<uint64_t> probe = keys;
  for (size_t i = N - 1; i > 0; --i) std::swap(probe[i], probe[g() % (i + 1)]);  // own shuffle, raw engine
  run<std::hash<uint64_t>>("std::hash", keys, probe);
  run<MsHash>("ms_hash", keys, probe);
}
