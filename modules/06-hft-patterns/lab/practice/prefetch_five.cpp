// prefetch_five.cpp - what each __builtin_prefetch(addr, rw, locality) form compiles to.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/prefetch_five.cpp -o -
//   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/06-hft-patterns/lab/practice/prefetch_five.cpp -o -   (then add -mprfchw, or -march=...)
// What does each __builtin_prefetch(addr, rw, locality) form compile to?
void r3(const void* p) { __builtin_prefetch(p, 0, 3); }
void r2(const void* p) { __builtin_prefetch(p, 0, 2); }
void r1(const void* p) { __builtin_prefetch(p, 0, 1); }
void r0(const void* p) { __builtin_prefetch(p, 0, 0); }
void w3(const void* p) { __builtin_prefetch(p, 1, 3); }
