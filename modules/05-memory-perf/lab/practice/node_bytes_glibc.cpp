// node_bytes_glibc.cpp - Linux/glibc version of node_bytes.cpp (malloc_usable_size replaces macOS malloc_size),
// plus step 3: the address step between consecutive blocks of the same request size.
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes_glibc.cpp -o "${TMPDIR:-/tmp}/node_bytes_glibc" && "${TMPDIR:-/tmp}/node_bytes_glibc"
// Step 1: a counting allocator records the bytes the container asks for, and the pointer it got.
// Step 2: ask the allocator (glibc malloc_usable_size) how many bytes that same live block really occupies.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <malloc.h>
#define malloc_size malloc_usable_size  // glibc's size query

static std::size_t last_request = 0;
static void* last_ptr = nullptr;
template <class T> struct Counting {
  using value_type = T;
  Counting() = default;
  template <class U> Counting(const Counting<U>&) {}
  T* allocate(std::size_t n) {
    last_request = n * sizeof(T);
    last_ptr = std::malloc(n * sizeof(T));
    return static_cast<T*>(last_ptr);
  }
  void deallocate(T* p, std::size_t) { std::free(p); }
  template <class U> bool operator==(const Counting<U>&) const { return true; }
};
struct Rec40 { std::uint64_t a[5]; };   // 40 bytes

static void report(const char* what, std::size_t value_bytes) {
  // the node is still alive here: malloc_size of the node's own block
  std::printf("%-16s value=%zu  requested per node=%zu  malloc_usable_size(node)=%zu\n", what, value_bytes, last_request,
              malloc_size(last_ptr));
}

// Step 3: malloc_usable_size may leave out a per-block header. Allocate many blocks of each node's
// request size and print the most common address step between consecutive blocks.
static void spacing(std::size_t req) {
  std::map<long, int> steps;
  char* prev = static_cast<char*>(std::malloc(req));
  for (int i = 0; i < 100000; ++i) {
    char* p = static_cast<char*>(std::malloc(req));
    // subtract addresses as integers: subtracting pointers from different allocations is UB
    steps[static_cast<long>(reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(prev))]++;
    prev = p;
  }
  long mode = 0; int best = 0;
  for (auto [d, c] : steps) if (c > best) { best = c; mode = d; }
  std::printf("request %3zu: malloc_usable_size=%zu  most common address step=%ld (%d of 100000)\n", req,
              malloc_size(prev), mode, best);
}
int main() {
  { std::list<Rec40, Counting<Rec40>> l; l.push_back({}); report("list<Rec40>", sizeof(Rec40)); }
  { std::map<int, Rec40, std::less<int>, Counting<std::pair<const int, Rec40>>> m; m[1]; report("map<int,Rec40>", sizeof(std::pair<const int, Rec40>)); }
  { std::list<std::uint64_t, Counting<std::uint64_t>> l; l.push_back(1); report("list<uint64_t>", 8); }
  for (std::size_t req : {56, 80, 24}) spacing(req);
#ifdef _LIBCPP_VERSION
  std::printf("standard library: libc++ %d\n", _LIBCPP_VERSION);
#endif
#ifdef __GLIBCXX__
  std::printf("standard library: libstdc++ %d\n", __GLIBCXX__);
#endif
}
