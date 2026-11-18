// mini_bench.cpp -- module 07 lesson 03: measuring WITHOUT Google Benchmark.
// Build and run (from the tutorial root; no setup step needed):
//   clang++ -std=c++20 -O2 modules/07-architecture/lab/bench/mini_bench.cpp -o "${TMPDIR:-/tmp}/mini_bench"
//   "${TMPDIR:-/tmp}/mini_bench"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// Same idea as benchmark::DoNotOptimize on GCC/Clang: an empty asm statement
// that claims to read and modify `value`, so the compiler must keep it real.
template <class T>
inline void escape(T& value) { asm volatile("" : "+r,m"(value) : : "memory"); }

static int add(int a, int b) { return a + b; }

int main() {
    using clock = std::chrono::steady_clock;
    constexpr long kIters = 100'000'000;
    std::vector<double> runs;
    for (int rep = 0; rep < 5; ++rep) {           // repeat: one run is not a measurement
        int a = 2, b = 3;
        auto t0 = clock::now();
        for (long i = 0; i < kIters; ++i) {
            escape(a);
            escape(b);
            int r = add(a, b);
            escape(r);
        }
        auto t1 = clock::now();
        runs.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters);
    }
    std::sort(runs.begin(), runs.end());
    std::printf("ns per iteration over 5 runs: min %.3f  median %.3f  max %.3f\n",
                runs.front(), runs[2], runs.back());
}
