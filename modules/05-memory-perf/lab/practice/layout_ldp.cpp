// layout_ldp.cpp - same arithmetic, two field orders. Does the instruction count change?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/layout_ldp.cpp -o -
#include <cstdint>
struct RingA { uint64_t head, mask, tail; };   // head and mask adjacent
struct RingB { uint64_t head, tail, mask; };   // head and mask 16 bytes apart
extern "C" uint64_t slot_a(const RingA& r) { return r.head & r.mask; }
extern "C" uint64_t slot_b(const RingB& r) { return r.head & r.mask; }
