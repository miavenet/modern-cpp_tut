// modmask_neg.cpp - run the signed cases of modmask.cpp with x = -5 and n = 1024 (n passed at run time).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/modmask.cpp modules/05-memory-perf/lab/practice/modmask_neg.cpp -o "${TMPDIR:-/tmp}/modmask_neg" && "${TMPDIR:-/tmp}/modmask_neg"
#include <cstdint>
#include <cstdio>
extern "C" int64_t mod_s_const(int64_t); extern "C" int64_t mask_s_const(int64_t);
extern "C" int64_t mod_s_rt(int64_t, int64_t); extern "C" int64_t mask_s_rt(int64_t, int64_t);
int main(int argc, char**) { int64_t x = -5, n = 1024 + argc - 1;
  printf("x=-5 n=1024: mod_s_const=%lld mask_s_const=%lld mod_s_rt=%lld mask_s_rt=%lld\n",
   (long long)mod_s_const(x), (long long)mask_s_const(x), (long long)mod_s_rt(x, n), (long long)mask_s_rt(x, n)); }
