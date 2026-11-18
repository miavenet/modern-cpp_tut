// hashloop.cpp - one serial chain of 12 hash steps; is the loop unrolled?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -Rpass=loop-unroll
//   g++ -std=c++20 -O2 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -fopt-info-loop   (GCC; then -O3)
//   clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/hashloop.cpp -o -   (count backward branches; then -O1)
#include <cstdint>
// One serial chain of 12 steps; each step needs the previous h.
std::uint32_t mix(const std::uint32_t* p, std::uint32_t h) {
  for (int i = 0; i < 12; ++i)
    h = h * 31u + p[i];
  return h;
}
