// overaligned_new.cpp - compile only: does the compiler tell you that plain new cannot honour alignas(128)?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++14 -Wall -Wextra -c modules/05-memory-perf/lab/practice/overaligned_new.cpp -o "${TMPDIR:-/tmp}/oa.o"   (then add -Wover-aligned; then -std=c++17)
//   g++ -std=c++14 -Wall -Wextra -c modules/05-memory-perf/lab/practice/overaligned_new.cpp -o "${TMPDIR:-/tmp}/oa.o"   (GCC; then -std=c++17)
// Compile-only check: does the compiler tell you that plain `new` cannot honour alignas(128)?
#include <atomic>
#include <cstdio>
struct alignas(128) Slot { std::atomic<long> n{0}; };
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
static_assert(alignof(Slot) > __STDCPP_DEFAULT_NEW_ALIGNMENT__, "Slot is over-aligned for plain new");
#endif
Slot* make() { return new Slot; }   // C++14: plain operator new(size_t). C++17: aligned overload.
int main() {
  Slot* s = make();
  std::printf("alignof=%zu\n", alignof(Slot));
  delete s;
}
