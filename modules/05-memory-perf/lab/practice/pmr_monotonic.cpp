// pmr_monotonic.cpp - monotonic_buffer_resource: deallocate does nothing, so alloc/free in a loop grows memory.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/pmr_monotonic.cpp -o "${TMPDIR:-/tmp}/pmr_monotonic" && "${TMPDIR:-/tmp}/pmr_monotonic"
#include <cstdio>
#include <memory_resource>
struct Counting : std::pmr::memory_resource {
  size_t bytes = 0, calls = 0;
  void* do_allocate(size_t n, size_t a) override { bytes += n; ++calls; return std::pmr::new_delete_resource()->allocate(n, a); }
  void do_deallocate(void* p, size_t n, size_t a) override { std::pmr::new_delete_resource()->deallocate(p, n, a); }
  bool do_is_equal(const memory_resource& o) const noexcept override { return this == &o; }
};
int main() {
  Counting up; std::pmr::monotonic_buffer_resource mono(&up);
  std::pmr::memory_resource* r = &mono;
  for (int round = 1; round <= 3; ++round) {
    for (int i = 0; i < 100000; ++i) { void* p = r->allocate(64, 16); r->deallocate(p, 64, 16); }
    printf("after round %d: upstream calls=%zu upstream bytes=%zu\n", round, up.calls, up.bytes);
  }
  mono.release(); printf("release() returns everything to upstream\n");
}
