// line_consts.cpp - compile-time cache-line constants vs the running machine, and the size of std::mutex.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_consts.cpp -o "${TMPDIR:-/tmp}/line_consts" && "${TMPDIR:-/tmp}/line_consts"
//   sysctl hw.cachelinesize   (macOS)   /   getconf LEVEL1_DCACHE_LINESIZE   (Linux)
// Compile-time cache-line guess vs the running machine; and how big is std::mutex here?
#include <cstdio>
#include <mutex>
#include <new>
int main() {
#ifdef __cpp_lib_hardware_interference_size
  std::printf("destructive=%zu constructive=%zu\n", std::hardware_destructive_interference_size,
              std::hardware_constructive_interference_size);
#else
  std::puts("__cpp_lib_hardware_interference_size not defined");
#endif
  std::printf("sizeof(std::mutex)=%zu alignof=%zu\n", sizeof(std::mutex), alignof(std::mutex));
}
