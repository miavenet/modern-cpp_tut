// fsum.cpp - three ways to add up a float array: which may become vector adds?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O3 -fexperimental-library -S modules/05-memory-perf/lab/practice/fsum.cpp -o "${TMPDIR:-/tmp}/fsum.s" && sh modules/05-memory-perf/lab/practice/fadd_kinds.sh "${TMPDIR:-/tmp}/fsum.s"   (Apple clang)
//   g++ -std=c++20 -O3 -S modules/05-memory-perf/lab/practice/fsum.cpp -o "${TMPDIR:-/tmp}/fsum.s" && sh modules/05-memory-perf/lab/practice/fadd_kinds.sh "${TMPDIR:-/tmp}/fsum.s"   (Linux; then again with -fopenmp-simd; same with clang++)
// Three ways to add up a float array. Which ones may the compiler turn into vector adds?
#include <cstddef>
#include <execution>
#include <numeric>

float sum_accumulate(const float* p, std::size_t n) {
    return std::accumulate(p, p + n, 0.0f);
}
float sum_reduce(const float* p, std::size_t n) {
    return std::reduce(p, p + n, 0.0f);
}
float sum_reduce_unseq(const float* p, std::size_t n) {
    return std::reduce(std::execution::unseq, p, p + n, 0.0f);
}
