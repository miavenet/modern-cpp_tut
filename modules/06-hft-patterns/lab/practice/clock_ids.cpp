// clock_ids.cpp - 1000 clock_gettime calls with the clock id index given on the command line (0-5; Linux only).
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_ids.cpp -o "${TMPDIR:-/tmp}/clock_ids" && for id in 0 1 2 3 4 5; do strace -c -e trace=clock_gettime "${TMPDIR:-/tmp}/clock_ids" $id; done
// 1000 reads of each clock id; count kernel entries with strace -c.
#include <time.h>
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
    clockid_t ids[] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW, CLOCK_BOOTTIME, CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID};
    int which = argc > 1 ? std::atoi(argv[1]) : 0;
    long sink = 0;
    for (int i = 0; i < 1000; ++i) { timespec t; clock_gettime(ids[which], &t); sink += t.tv_nsec & 1; }
    std::printf("id_index=%d sink=%ld\n", which, sink);
}
