// interference_macro.cpp - print the interference-size feature macro and both constants, or say they are missing.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/interference_macro.cpp -o "${TMPDIR:-/tmp}/interference_macro" && "${TMPDIR:-/tmp}/interference_macro"   (then g++ the same way)
// What does the toolchain think the interference size is?
#include <cstdio>
#include <new>
int main() {
#ifdef __cpp_lib_hardware_interference_size
  std::printf("__cpp_lib_hardware_interference_size=%ld destructive=%zu constructive=%zu\n",
              (long)__cpp_lib_hardware_interference_size,
              std::hardware_destructive_interference_size,
              std::hardware_constructive_interference_size);
#else
  std::puts("__cpp_lib_hardware_interference_size not defined");
#endif
}
