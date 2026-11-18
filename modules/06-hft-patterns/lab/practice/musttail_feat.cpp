// musttail_feat.cpp - feature test and diagnostic for a statement attribute on return (compile on Compiler Explorer, -O1 -std=c++20).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O1 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/musttail_feat.cpp -o -
// Feature-test and diagnostic for a statement attribute on return.
int has_clang_musttail = __has_cpp_attribute(clang::musttail);
int has_gnu_musttail   = __has_cpp_attribute(gnu::musttail);
int step(int n, int acc);
int step(int n, int acc) {
  if (n == 0) return acc;
  [[clang::musttail]] return step(n - 1, acc + 1);
}
