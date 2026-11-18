// fill_optout.cpp - a local array filled by another file: does a per-variable opt-out remove the forced fill?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++2c -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -   (count memset/bzero calls per function)
//   g++ -std=c++23 -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -   (GCC; again with -fno-ipa-icf)
// Opaque-fill local array: does a per-variable opt-out remove the forced fill?
#include <cstdint>
void consume(const uint32_t* h, int n);      // opaque
void filled_by_callee(uint32_t* h, int n);   // opaque
void plain() {
  uint32_t hist[256];
  filled_by_callee(hist, 256); consume(hist, 256);
}
void opted_out() {
  uint32_t hist[256] __attribute__((uninitialized));
  filled_by_callee(hist, 256); consume(hist, 256);
}
#ifdef TRY_STD
void std_opted_out() {
  uint32_t hist[256] [[indeterminate]];
  filled_by_callee(hist, 256); consume(hist, 256);
}
#endif
