// pool_size.cpp - what a "size the pool to the machine" harness sees when you restrict the CPUs it may run on (the affinity line is Linux only).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/pool_size.cpp -o "${TMPDIR:-/tmp}/pool_size" && "${TMPDIR:-/tmp}/pool_size"
//   taskset -c 0 "${TMPDIR:-/tmp}/pool_size"   (Linux)
// What does a "size the pool to the machine" harness see when you restrict
// the CPUs it may run on?
#include <cstdio>
#include <thread>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#endif
int main() {
    std::printf("std::thread::hardware_concurrency() = %u\n", std::thread::hardware_concurrency());
    std::printf("sysconf(_SC_NPROCESSORS_ONLN)       = %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof set, &set) == 0)
        std::printf("CPUs in my affinity mask            = %d\n", CPU_COUNT(&set));
#endif
}
