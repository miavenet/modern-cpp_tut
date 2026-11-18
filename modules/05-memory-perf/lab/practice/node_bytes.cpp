// node_bytes.cpp - what does one node of a node-based container really cost? (macOS only: malloc_size)
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes.cpp -o "${TMPDIR:-/tmp}/node_bytes" && "${TMPDIR:-/tmp}/node_bytes"
// Step 1: a counting allocator records the bytes the container asks for, and the pointer it got.
// Step 2: ask the allocator (macOS malloc_size) how many bytes that same live block really occupies.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <malloc/malloc.h>

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
  std::printf("%-16s value=%zu  requested per node=%zu  malloc_size(node)=%zu\n", what, value_bytes, last_request,
              malloc_size(last_ptr));
}
int main() {
  { std::list<Rec40, Counting<Rec40>> l; l.push_back({}); report("list<Rec40>", sizeof(Rec40)); }
  { std::map<int, Rec40, std::less<int>, Counting<std::pair<const int, Rec40>>> m; m[1]; report("map<int,Rec40>", sizeof(std::pair<const int, Rec40>)); }
  { std::list<std::uint64_t, Counting<std::uint64_t>> l; l.push_back(1); report("list<uint64_t>", 8); }
#ifdef _LIBCPP_VERSION
  std::printf("standard library: libc++ %d\n", _LIBCPP_VERSION);
#endif
}
