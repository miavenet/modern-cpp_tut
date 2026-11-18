// hdis.cpp - what this toolchain says std::hardware_{destructive,constructive}_interference_size are.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/hdis.cpp -o "${TMPDIR:-/tmp}/hdis" && "${TMPDIR:-/tmp}/hdis"
//   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/05-memory-perf/lab/practice/hdis.cpp -o -   (the x86-64 values appear as constants)
#include <cstdio>
#include <new>
int main() {
#ifdef __cpp_lib_hardware_interference_size
  std::printf("__cpp_lib_hardware_interference_size=%ld destructive=%zu constructive=%zu\n",
              (long)__cpp_lib_hardware_interference_size,
              std::hardware_destructive_interference_size,
              std::hardware_constructive_interference_size);
#else
  std::printf("__cpp_lib_hardware_interference_size not defined\n");
#endif
}
