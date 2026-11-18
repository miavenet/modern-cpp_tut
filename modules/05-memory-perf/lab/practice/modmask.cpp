// modmask.cpp - is `x & (n-1)` cheaper than `x % n` for a power-of-two n? Read the listing.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/modmask.cpp -o -
// Four cases: unsigned vs signed x, compile-time vs runtime n. modmask_neg.cpp runs the signed ones.
#include <cstdint>
extern "C" uint64_t mod_u_const(uint64_t x)  { return x % 1024; }
extern "C" uint64_t mask_u_const(uint64_t x) { return x & 1023; }
extern "C" int64_t  mod_s_const(int64_t x)   { return x % 1024; }   // result keeps the sign of x
extern "C" int64_t  mask_s_const(int64_t x)  { return x & 1023; }   // differs from % for negative x
extern "C" uint64_t mod_u_rt(uint64_t x, uint64_t n)  { return x % n; }        // n not known at compile time
extern "C" uint64_t mask_u_rt(uint64_t x, uint64_t n) { return x & (n - 1); }  // caller promises n is a power of two
extern "C" int64_t  mod_s_rt(int64_t x, int64_t n)    { return x % n; }        // signed, runtime n
extern "C" int64_t  mask_s_rt(int64_t x, int64_t n)   { return x & (n - 1); }  // differs from % for negative x
