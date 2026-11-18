// prefetch_forms.cpp - which instruction each __builtin_prefetch form becomes, arm64 and baseline x86-64.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ --target=arm64-apple-macos -std=c++20 -O2 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -
//   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -   (then add -mprfchw)
// Which instruction does each __builtin_prefetch form become?

void read_l0(const void* p)  { __builtin_prefetch(p, 0, 0); }
void read_l1(const void* p)  { __builtin_prefetch(p, 0, 1); }
void read_l2(const void* p)  { __builtin_prefetch(p, 0, 2); }
void read_l3(const void* p)  { __builtin_prefetch(p, 0, 3); }
void write_l3(const void* p) { __builtin_prefetch(p, 1, 3); }
