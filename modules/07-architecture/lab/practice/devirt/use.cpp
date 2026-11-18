// use.cpp - does the virtual call stay indirect? Look for blr (indirect) vs bl (direct).
// Build from the tutorial root (D = A with `final` removed from shape.h, in a copy):
//   (A) clang++ -std=c++20 -O2 -c modules/07-architecture/lab/practice/devirt/use.cpp -o "${TMPDIR:-/tmp}/use.o" && objdump -d -r "${TMPDIR:-/tmp}/use.o"
//   (B) clang++ -std=c++20 -O2 -flto modules/07-architecture/lab/practice/devirt/use.cpp modules/07-architecture/lab/practice/devirt/shape.cpp -o "${TMPDIR:-/tmp}/B" && objdump -d "${TMPDIR:-/tmp}/B"
//   (C) clang++ -std=c++20 -O2 -flto -fwhole-program-vtables -fvisibility=hidden modules/07-architecture/lab/practice/devirt/use.cpp modules/07-architecture/lab/practice/devirt/shape.cpp -o "${TMPDIR:-/tmp}/C" && objdump -d "${TMPDIR:-/tmp}/C"
#include "shape.h"
__attribute__((noinline)) long sum_iface(Shape& s, const long* v, int n)   { long t = 0; for (int i = 0; i < n; ++i) t += s.area(v[i]); return t; }
__attribute__((noinline)) long sum_leaf(Square& s, const long* v, int n)   { long t = 0; for (int i = 0; i < n; ++i) t += s.area(v[i]); return t; }
int main(int argc, char**) {
  long v[4] = {1, 2, 3, argc};
  long a = sum_iface(make_shape(), v, 4);
  long b = sum_leaf(make_square(), v, 4);
  return int((a + b) & 0x7f);
}
