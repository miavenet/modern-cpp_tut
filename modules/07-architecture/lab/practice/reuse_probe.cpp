// reuse_probe.cpp - 257 rounds: does the next same-size allocation return the address just freed? (allocations go through reuse_shim.cpp)
// Build and run from the tutorial root; the program goes to your temp directory:
//   bash modules/07-architecture/lab/practice/reuse_check.sh clang++   (or: g++)
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
extern "C" void* request_bytes(std::size_t);
extern "C" void retire_bytes(void*);
int main() {
  unsigned same=0;
  for (unsigned i=0;i<257;++i) {
    void* p=request_bytes(32); if (!p) return 2;
    const std::uintptr_t old_address=reinterpret_cast<std::uintptr_t>(p);
    retire_bytes(p);
    void* q=request_bytes(32); if (!q) return 2;
    same += reinterpret_cast<std::uintptr_t>(q)==old_address;
    retire_bytes(q);
  }
  std::printf("reuse=%u/257\n",same);
}
