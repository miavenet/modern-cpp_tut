// bench_add.cpp -- module 07 lesson 03. Two benchmarks of "add two numbers".
// Build (from the tutorial root, after ./setup.sh --benchmark):
//   clang++ -std=c++20 -O2 -I third_party/google-benchmark/include \
//     modules/07-architecture/lab/bench/bench_add.cpp \
//     -L third_party/google-benchmark/lib -lbenchmark -o "${TMPDIR:-/tmp}/bench_add"
//   "${TMPDIR:-/tmp}/bench_add"
#include <benchmark/benchmark.h>
#include <vector>

static int add(int a, int b) { return a + b; }

// Inputs are compile-time constants: the compiler may compute add(2, 3) once,
// at compile time, and the loop then measures almost nothing.
static void BM_AddConstant(benchmark::State& state) {
    for (auto _ : state) {
        int r = add(2, 3);
        benchmark::DoNotOptimize(r);   // keeps r, but not the work that made r
    }
}
BENCHMARK(BM_AddConstant);

// Inputs pass through DoNotOptimize first, so the compiler must treat them
// as unknown values and really perform the addition every iteration.
static void BM_AddOpaque(benchmark::State& state) {
    int a = 2, b = 3;
    for (auto _ : state) {
        benchmark::DoNotOptimize(a);
        benchmark::DoNotOptimize(b);
        int r = add(a, b);
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_AddOpaque);

// Adapted from Google Benchmark v1.9.5, docs/user_guide.md.
// Copyright Google Benchmark Authors.
// Licensed under Apache-2.0; see LICENSES/Apache-2.0.txt.
// Modified for this tutorial.
static void BM_VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<int> v;
        v.reserve(1);
        benchmark::DoNotOptimize(v.data());  // let the vector's memory "escape"
        v.push_back(42);
        benchmark::ClobberMemory();          // force the write to really happen
    }
}
BENCHMARK(BM_VectorPushBack);

BENCHMARK_MAIN();
