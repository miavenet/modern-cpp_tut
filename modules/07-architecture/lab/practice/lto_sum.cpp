// lto_sum.cpp - the summing loop that hands &acc to observe(), which lives in lto_observe.cpp.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/lto_sum.cpp modules/07-architecture/lab/practice/lto_observe.cpp -o "${TMPDIR:-/tmp}/lto_off" && objdump -d "${TMPDIR:-/tmp}/lto_off"
//   clang++ -std=c++20 -O2 -flto modules/07-architecture/lab/practice/lto_sum.cpp modules/07-architecture/lab/practice/lto_observe.cpp -o "${TMPDIR:-/tmp}/lto_on" && objdump -d "${TMPDIR:-/tmp}/lto_on"
// Same loop as sum_escape_each in escape_opaque.cpp (module 05); observe() lives in lto_observe.cpp.
#include <cstdio>
void observe(const int* p);
__attribute__((noinline)) int sum_escape_each(const int* a, int n) {
    int acc = 0;
    for (int i = 0; i < n; ++i) { acc += a[i]; observe(&acc); }
    return acc;
}
int main(int argc, char**) {
    static int data[1000];
    for (int i = 0; i < 1000; ++i) data[i] = i * argc;
    std::printf("%d\n", sum_escape_each(data, 1000));
}
