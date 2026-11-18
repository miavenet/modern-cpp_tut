// line_size.cpp - what does the library think a cache line is, and what does the OS report?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_size.cpp -o "${TMPDIR:-/tmp}/line_size" && "${TMPDIR:-/tmp}/line_size" && sysctl hw.cachelinesize
#include <cstdio>
#include <new>
#include <version>

int main() {
#ifdef __cpp_lib_hardware_interference_size
  std::printf("__cpp_lib_hardware_interference_size=%ld\n", (long)__cpp_lib_hardware_interference_size);
  std::printf("hardware_destructive_interference_size=%zu\n", std::hardware_destructive_interference_size);
  std::printf("hardware_constructive_interference_size=%zu\n", std::hardware_constructive_interference_size);
#else
  std::printf("__cpp_lib_hardware_interference_size not defined\n");
#endif
}
