// wrap_constexpr.cpp - does a counter that wraps have defined behaviour? Let constant evaluation answer.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -fsyntax-only modules/02-type-system/lab/practice/wrap_constexpr.cpp
//   clang++ -std=c++20 -fsyntax-only -DSIGNED modules/02-type-system/lab/practice/wrap_constexpr.cpp
// Does a counter that wraps have defined behaviour? Let constant evaluation answer.
#include <climits>
#include <cstdint>
template <class T>
constexpr T bump_twice(T v) { v += 1; v += 1; return v; }
// unsigned: arithmetic is modulo 2^N, so wrapping is defined
static_assert(bump_twice<std::uint32_t>(UINT32_MAX) == 1u);
#ifdef SIGNED
// signed: overflow is undefined; a constant expression may not contain UB
static_assert(bump_twice<int>(INT_MAX) == INT_MIN + 1);
#endif
int main() {}
