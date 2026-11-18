// ticks.cpp -- read a hardware tick counter. Portable first, then per-CPU.
// Run from the tutorial root:
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/measure/ticks.cpp -o "${TMPDIR:-/tmp}/ticks" && "${TMPDIR:-/tmp}/ticks"
#include <chrono>
#include <cstdint>
#include <cstdio>

#if defined(__aarch64__)
// AArch64: the generic timer's virtual count register. ISB stops the read
// from being performed early, before the instructions that precede it.
inline std::uint64_t ticks() {
    std::uint64_t v;
    asm volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}
inline std::uint64_t ticks_per_second() {
    std::uint64_t f;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}
#elif defined(__x86_64__)
#include <x86intrin.h>
// x86-64: fence before and after the time-stamp counter read.
inline std::uint64_t ticks() {
    _mm_lfence();
    const auto t = __rdtsc();
    _mm_lfence();
    return t;
}
inline std::uint64_t ticks_per_second() { return 0; }   // Not queried here; supported CPUs may provide it through CPUID leaf 0x15.
#endif

int main() {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    std::uint64_t a = ticks();
    volatile std::uint64_t sink = 0;
    for (int i = 0; i < 10'000'000; ++i) sink = sink + i;
    std::uint64_t b = ticks();
    auto t1 = clock::now();
    std::printf("counter frequency: %llu ticks/s\n", (unsigned long long)ticks_per_second());
    std::printf("loop: %llu ticks, %lld us by steady_clock\n",
                (unsigned long long)(b - a),
                (long long)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
}
