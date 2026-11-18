// tsize.cpp - code bytes per template instantiation, object file vs linked binary.
// Build from the tutorial root (NTYPES = 1, 2, 4, 8, 16); output goes to your temp directory:
//   clang++ -std=c++20 -O2 -DNDEBUG -DNTYPES=8 -c modules/07-architecture/lab/practice/tsize.cpp -o "${TMPDIR:-/tmp}/t8.o"
//   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/tmain.cpp "${TMPDIR:-/tmp}/t8.o" -o "${TMPDIR:-/tmp}/b8"
//   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/tmain.cpp "${TMPDIR:-/tmp}/t8.o" -Wl,-no_deduplicate -o "${TMPDIR:-/tmp}/b8nd"
//   size -m "${TMPDIR:-/tmp}/t8.o" ; size -m "${TMPDIR:-/tmp}/b8" ; size -m "${TMPDIR:-/tmp}/b8nd"
// Each Rec<I> is a distinct type with the same layout; work<I> sorts, searches
// and erases in a std::vector<Rec<I>>.
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

template <int I> struct Rec {
  uint32_t id;
  int64_t qty;
  bool operator<(const Rec& o) const { return id < o.id; }
};

template <int I>
__attribute__((noinline)) long work(int n) {
  std::vector<Rec<I>> v;
  for (int i = 0; i < n; ++i) v.push_back(Rec<I>{(uint32_t)(i * 2654435761u), i});
  std::sort(v.begin(), v.end());
  long s = 0;
  for (int i = 0; i < n; ++i) {
    auto it = std::lower_bound(v.begin(), v.end(), Rec<I>{(uint32_t)i, 0});
    if (it != v.end()) s += it->qty;
  }
  v.erase(v.begin(), v.begin() + v.size() / 2);
  return s + (long)v.size();
}

template <int... Is> long all(int n, std::integer_sequence<int, Is...>) { return (work<Is>(n) + ...); }
long entry(int n) { return all(n, std::make_integer_sequence<int, NTYPES>{}); }
