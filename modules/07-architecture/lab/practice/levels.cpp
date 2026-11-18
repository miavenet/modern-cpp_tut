// levels.cpp - two loops, one a vectorization control; what does the vectorizer report say at each -O level?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   bash modules/07-architecture/lab/practice/levels_report.sh clang++   (and g++ on Linux)
// Two loops. The first can be vectorized. The second cannot as written:
// each element needs the value written one step earlier.
#include <cstddef>
#include <cstdint>

void add_fee(std::uint32_t* cost, std::size_t n, std::uint32_t fee) {
    for (std::size_t i = 0; i < n; ++i) cost[i] += fee;
}

void running_total(std::uint32_t* v, std::size_t n) {
    for (std::size_t i = 1; i < n; ++i) v[i] += v[i - 1];
}
