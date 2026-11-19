// clock_vs_getppid.cpp - 1000 clock_gettime(CLOCK_MONOTONIC) calls, then 1000 getppid() calls (Linux only).
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_vs_getppid.cpp -o "${TMPDIR:-/tmp}/clock_vs_getppid" && strace -c "${TMPDIR:-/tmp}/clock_vs_getppid"
// Two cheap-looking calls, 1000 times each. Which ones enter the kernel?
#include <time.h>
#include <unistd.h>
#include <cstdio>
int main() {
    long sink = 0;
    for (int i = 0; i < 1000; ++i) { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); sink += t.tv_nsec & 1; }
    for (int i = 0; i < 1000; ++i) sink += getppid() & 1;
    std::printf("sink=%ld\n", sink);
}
