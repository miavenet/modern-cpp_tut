// clock_tail.cpp -- module 06 lesson 03: how good is the clock you time tails with?
// Build and run (from the tutorial root):
//   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/06-hft-patterns/lab/measure/clock_tail.cpp -o "${TMPDIR:-/tmp}/clock_tail" && "${TMPDIR:-/tmp}/clock_tail"
//
// Times NOTHING 1,000,000 times (two back-to-back steady_clock reads) and prints the
// histogram. Every nonzero sample is the clock or the machine, never your code:
//   * p50 and p99 show the tick (the clock floor): anything shorter reads as 0 or 1 tick
//   * the far tail and max show the OS taking the core away (interrupts, preemption)
// Then the same with too few samples, to show what lat_hist refuses to print.
#include "lat_hist.hpp"

#include <cstdio>

int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("empty interval x 1,000,000; environment: clang %s, %s, one thread, no core "
                "pinning (macOS), shared machine\n", __clang_version__, opt);
    static lat::histogram h;
    for (int i = 0; i < 1'000'000; ++i) {
        const std::uint64_t t0 = lat::now_ns();
        h.record(lat::now_ns() - t0);
    }
    h.report(stdout, "empty, 1e6", floor_ns);
    h.reset();
    for (int i = 0; i < 5'000; ++i) {
        const std::uint64_t t0 = lat::now_ns();
        h.record(lat::now_ns() - t0);
    }
    h.report(stdout, "empty, 5e3", floor_ns);
    std::printf("p99.99 needs %llu samples, p99.9 needs %llu, p99 needs %llu "
                "(at least %llu samples beyond the rank)\n",
                static_cast<unsigned long long>(lat::min_samples(0.9999)),
                static_cast<unsigned long long>(lat::min_samples(0.999)),
                static_cast<unsigned long long>(lat::min_samples(0.99)),
                static_cast<unsigned long long>(lat::kTailSamples));
}
