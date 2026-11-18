// bytes_per_elem_glibc.cpp - Linux/glibc version of bytes_per_elem.cpp (same code; malloc_usable_size
// replaces macOS malloc_size). Heap bytes per element for a small record { uint32_t id; uint64_t qty; }.
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/bytes_per_elem_glibc.cpp -o "${TMPDIR:-/tmp}/bytes_per_elem_glibc" && "${TMPDIR:-/tmp}/bytes_per_elem_glibc"
// Raw data is 12 bytes, but sizeof(Rec) is 16 (padding for alignment).
// Two tallies per container, through a counting allocator:
//   requested = bytes the container asked for (n * sizeof(T) per allocation)
//   granted   = bytes malloc actually reserved (glibc malloc_usable_size), which
//               includes malloc's rounding up to its size classes
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <list>
#include <malloc.h>
#define malloc_size malloc_usable_size  // glibc's size query
#include <new>
#include <set>
#include <vector>

struct Rec {
  uint32_t id;
  uint64_t qty;
  bool operator<(const Rec& o) const { return id < o.id; }
};

static size_t req = 0, got = 0;
template <class T> struct Count {
  using value_type = T;
  Count() = default;
  template <class U> Count(const Count<U>&) {}
  T* allocate(size_t n) {
    void* p = std::malloc(n * sizeof(T));
    if (!p) throw std::bad_alloc();
    req += n * sizeof(T); got += malloc_size(p);
    return static_cast<T*>(p);
  }
  void deallocate(T* p, size_t n) { req -= n * sizeof(T); got -= malloc_size(p); std::free(p); }
  template <class U> bool operator==(const Count<U>&) const { return true; }
};
static void show(size_t n, const char* name) {
  std::printf("n=%8zu %-21s requested %6.2f  granted %6.2f bytes/element\n", n, name, (double)req / n,
              (double)got / n);
}

int main() {
  std::printf("sizeof(Rec) = %zu, alignof(Rec) = %zu, raw id+qty = %zu\n", sizeof(Rec), alignof(Rec),
              sizeof(uint32_t) + sizeof(uint64_t));
  for (size_t n : {1000u, 50000u, 1000000u}) {
    { req = got = 0; std::list<Rec, Count<Rec>> c;
      for (size_t i = 0; i < n; ++i) c.push_back({(uint32_t)i, i});
      show(n, "list"); }
    { req = got = 0; std::set<Rec, std::less<Rec>, Count<Rec>> c;
      for (size_t i = 0; i < n; ++i) c.insert({(uint32_t)(i * 2654435761u), i});
      show(n, "set"); }
    { req = got = 0; std::deque<Rec, Count<Rec>> c;
      for (size_t i = 0; i < n; ++i) c.push_back({(uint32_t)i, i});
      show(n, "deque"); }
    { req = got = 0; std::vector<Rec, Count<Rec>> c; c.reserve(n);
      for (size_t i = 0; i < n; ++i) c.push_back({(uint32_t)i, i});
      show(n, "vector after reserve"); }
    { req = got = 0; std::vector<Rec, Count<Rec>> c;
      for (size_t i = 0; i < n; ++i) c.push_back({(uint32_t)i, i});
      show(n, "vector, no reserve"); }
  }
}
