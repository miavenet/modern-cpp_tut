// ifc.cpp - if constexpr on a function parameter.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -fsyntax-only modules/02-type-system/lab/practice/ifc.cpp
// Gotcha: if constexpr needs a constant expression.
int scale(int qty, bool big) {
  if constexpr (big) return qty * 100;   // big is a function parameter
  return qty;
}
int main() { return scale(3, true) == 300 ? 0 : 1; }
