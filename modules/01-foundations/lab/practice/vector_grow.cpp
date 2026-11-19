// vector_grow.cpp - count allocations and element copies/moves while a vector grows (move not noexcept, noexcept, reserve).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/vector_grow.cpp -o "${TMPDIR:-/tmp}/vector_grow" && "${TMPDIR:-/tmp}/vector_grow"
// Count allocations and element copies/moves while a vector grows.
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

static long g_allocs = 0;
template <class T> struct CountingAlloc {
  using value_type = T;
  CountingAlloc() = default;
  template <class U> CountingAlloc(const CountingAlloc<U>&) {}
  T* allocate(std::size_t n) { ++g_allocs; return static_cast<T*>(::operator new(n * sizeof(T))); }
  void deallocate(T* p, std::size_t) { ::operator delete(p); }
  template <class U> bool operator==(const CountingAlloc<U>&) const { return true; }
};

static long g_copies = 0, g_moves = 0;
struct Quote {               // move ctor NOT noexcept
  long px = 0;
  Quote() = default;
  explicit Quote(long p) : px(p) {}
  Quote(const Quote& o) : px(o.px) { ++g_copies; }
  Quote(Quote&& o) : px(o.px) { ++g_moves; }
};
struct QuoteNx {             // same, but move ctor noexcept
  long px = 0;
  QuoteNx() = default;
  explicit QuoteNx(long p) : px(p) {}
  QuoteNx(const QuoteNx& o) : px(o.px) { ++g_copies; }
  QuoteNx(QuoteNx&& o) noexcept : px(o.px) { ++g_moves; }
};

template <class T> void run(const char* name, bool reserve, int n) {
  g_allocs = g_copies = g_moves = 0;
  std::vector<T, CountingAlloc<T>> v;
  if (reserve) v.reserve(n);
  long last_cap = -1;
  std::printf("%-8s reserve=%d n=%d caps:", name, reserve, n);
  for (int i = 0; i < n; ++i) {
    v.emplace_back(i);
    if ((long)v.capacity() != last_cap) { last_cap = (long)v.capacity(); std::printf(" %ld", last_cap); }
  }
  std::printf("\n         allocations=%ld copies=%ld moves=%ld\n", g_allocs, g_copies, g_moves);
}

int main(int argc, char** argv) {
  int n = argc > 1 ? std::atoi(argv[1]) : 1000;
  run<Quote>("Quote", false, n);
  run<QuoteNx>("QuoteNx", false, n);
  run<QuoteNx>("QuoteNx", true, n);
}
