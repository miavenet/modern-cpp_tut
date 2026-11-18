// prefetch_var.cpp - __builtin_prefetch with the locality passed from a variable.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/prefetch_var.cpp -o -
void f(const void* p, int k) { __builtin_prefetch(p, 0, k); }
