// overflow.cpp -- signed integer overflow is undefined behaviour.
// UBSan:  clang++ -std=c++20 -g -fsanitize=undefined modules/07-architecture/lab/sanitizers/overflow.cpp -o "${TMPDIR:-/tmp}/ovf" && "${TMPDIR:-/tmp}/ovf"
#include <climits>
#include <cstdio>
int main(int argc, char**) {
    int qty = INT_MAX - 1 + argc;   // INT_MAX when run with no arguments
    qty += 1;                       // overflow
    std::printf("qty = %d\n", qty);
}
