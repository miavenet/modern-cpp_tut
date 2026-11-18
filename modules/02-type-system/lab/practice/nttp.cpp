// nttp.cpp - a floating-point value as a template argument.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++17 -O2 -Wall -Wextra modules/02-type-system/lab/practice/nttp.cpp -o "${TMPDIR:-/tmp}/nttp" && "${TMPDIR:-/tmp}/nttp"
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/nttp.cpp -o "${TMPDIR:-/tmp}/nttp" && "${TMPDIR:-/tmp}/nttp"
#include <cstdio>
// A floating-point value as a template argument.
template <double Factor> double scaled(double x) { return x * Factor; }
int main() {
  std::printf("%.2f %.2f\n", scaled<0.5>(3.0), scaled<1.5>(2.0));
}
