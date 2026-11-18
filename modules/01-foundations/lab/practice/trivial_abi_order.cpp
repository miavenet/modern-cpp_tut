// trivial_abi_order.cpp - who destroys a by-value parameter, and when?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/plain" && "${TMPDIR:-/tmp}/plain"
//   clang++ -std=c++20 -O2 -DUSE_TRIVIAL_ABI modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/tabi" && "${TMPDIR:-/tmp}/tabi"
// Same program with and without [[clang::trivial_abi]] on the parameter type.
#include <cstdio>
#ifdef USE_TRIVIAL_ABI
#define TABI [[clang::trivial_abi]]
#else
#define TABI
#endif
struct TABI Owner {
  int* p;
  explicit Owner(int* q) : p(q) {}
  Owner(Owner&& o) noexcept : p(o.p) { o.p = nullptr; }
  ~Owner() { std::puts(p ? "  ~Owner (owning)" : "  ~Owner (empty)"); delete p; }
};
__attribute__((noinline)) void take(Owner o) { std::puts("  in take()"); }
int main() {
  std::puts("start of full-expression");
  (take(Owner(new int(1))), std::puts("  rest of the same full-expression"));
  std::puts("after full-expression");
}
