// prefetch_six.cpp - six __builtin_prefetch forms (read 3,2,1,0; write 3,0): which instruction does each become?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/prefetch_six.cpp -o - | grep -E 'prefetch|prfm'
//   x86-64 on Compiler Explorer: -O2, -O2 -march=x86-64-v3, -O2 -mprfchw, -O2 -march=skylake, -O2 -march=znver3
// Which instruction does each __builtin_prefetch(addr, rw, locality) become?
void pf_r3(const int* p) { __builtin_prefetch(p, 0, 3); }
void pf_r2(const int* p) { __builtin_prefetch(p, 0, 2); }
void pf_r1(const int* p) { __builtin_prefetch(p, 0, 1); }
void pf_r0(const int* p) { __builtin_prefetch(p, 0, 0); }
void pf_w3(int* p)       { __builtin_prefetch(p, 1, 3); }
void pf_w0(int* p)       { __builtin_prefetch(p, 1, 0); }
