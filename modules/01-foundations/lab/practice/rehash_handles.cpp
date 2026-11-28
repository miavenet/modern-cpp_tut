// rehash_handles.cpp - save pointers to seven unordered_map values, rehash to a larger bucket array, then write through a saved pointer.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/rehash_handles.cpp -o "${TMPDIR:-/tmp}/rehash_handles" && "${TMPDIR:-/tmp}/rehash_handles"
// Exits 1 if the buckets did not grow, a saved pointer moved, or the write did not reach key 39.
#include <unordered_map>
#include <array>
#include <cstdio>
int main() {
  std::unordered_map<int, int> m;
  std::array<int*, 7> saved{};
  for (int i=0; i<7; ++i) { m.emplace(i*13, 80+i); saved[i]=&m.at(i*13); }
  auto before=m.bucket_count();
  m.rehash(before*11+1);
  unsigned changed=0;
  for (int i=0; i<7; ++i) changed += saved[i]!=&m.at(i*13);
  *saved[3]=901;
  std::printf("bucket_growth=%d changed=%u edited=%d\n",
      m.bucket_count()>before,changed,m.at(39));
  return !(m.bucket_count()>before && changed==0 && m.at(39)==901);
}
