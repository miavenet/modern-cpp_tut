// fixed_false_nondep.cpp - the same, plus a template whose discarded operand does not depend on the template.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -fsyntax-only modules/02-type-system/lab/practice/fixed_false_nondep.cpp
#include <cstdio>
struct Plain { int id; };
template <class T> int fixed_false(const T& x) {
    if constexpr (false) return x.key();   // condition not dependent; operand is
    else return x.id;
}
template <class T> int nondep(const T&) {
    Plain p{1};
    if constexpr (false) return p.key();   // non-dependent operand: checked at definition
    else return 0;
}
int main() { std::printf("fixed_false=%d\n", fixed_false(Plain{7})); }
