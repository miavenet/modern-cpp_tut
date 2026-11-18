// clock_step.cpp - the declared clock period vs the smallest non-zero step seen between two back-to-back reads.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_step.cpp -o "${TMPDIR:-/tmp}/clock_step" && "${TMPDIR:-/tmp}/clock_step"
// What does your clock really tick at? Print the declared period and the smallest
// non-zero step actually observed between two back-to-back reads.
#include <chrono>
#include <cstdint>
#include <cstdio>
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
int main() {
    using C = std::chrono::steady_clock;
    std::printf("steady_clock period = %lld/%lld s\n", (long long)C::period::num, (long long)C::period::den);
    long long best = -1;
    for (int i = 0; i < 1'000'000; ++i) {
        auto a = C::now(), b = C::now();
        while (b == a) b = C::now();
        long long d = std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
        if (best < 0 || d < best) best = d;
    }
    std::printf("smallest observed steady_clock step = %lld ns\n", best);
#if defined(__APPLE__)
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    std::printf("mach_timebase_info numer/denom = %u/%u (ns per tick)\n", tb.numer, tb.denom);
#endif
#if defined(__aarch64__)
    std::uint64_t f; asm volatile("mrs %0, cntfrq_el0" : "=r"(f));
    std::printf("cntfrq_el0 = %llu Hz\n", (unsigned long long)f);
#endif
}
