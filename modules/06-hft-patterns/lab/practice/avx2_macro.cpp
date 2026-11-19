// avx2_macro.cpp - which branch the preprocessor chose for __AVX2__, and what the CPU says at run time (x86-64 only).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   x86-64 gcc on Compiler Explorer, flags -O2, then -O2 -mavx2; compare the assembly
// Is __AVX2__ a property of the CPU that runs this, or of the build flags?
#include <cstdio>
int main() {
#ifdef __AVX2__
  std::puts("built with __AVX2__ defined");
#else
  std::puts("built without __AVX2__");
#endif
  __builtin_cpu_init();
  std::printf("this CPU supports avx2 at run time: %d\n", __builtin_cpu_supports("avx2") ? 1 : 0);
}
