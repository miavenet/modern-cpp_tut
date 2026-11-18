// fixed_false.cpp - if constexpr (false) inside a template, with an operand that depends on the template.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/fixed_false.cpp -o "${TMPDIR:-/tmp}/fixed_false" && "${TMPDIR:-/tmp}/fixed_false"
#include <cstdio>
struct Plain { int id; };
template <class T> int fixed_false(const T& x) { if constexpr (false) return x.key(); else return x.id; }
int main() { std::printf("fixed_false=%d\n", fixed_false(Plain{7})); }
