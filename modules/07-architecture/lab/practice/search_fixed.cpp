// search_fixed.cpp - search-only early exit, two ways to hand over the data.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   bash modules/07-architecture/lab/practice/search_report.sh clang++   (and g++ O3 on Linux)
// Search-only early exit, two ways to hand over the data.
#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::int32_t SENTINEL = INT32_MIN;

// (1) pointer + run-time length: the compiler cannot know how much memory is readable.
std::size_t first_fault_ptr(const std::int32_t* in, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] == SENTINEL) return i;
    return n;
}

// (2) reference to a fixed-size array: all 1024 elements are known to be readable.
std::size_t first_fault_arr(const std::array<std::int32_t, 1024>& in) {
    for (std::size_t i = 0; i < in.size(); ++i)
        if (in[i] == SENTINEL) return i;
    return in.size();
}
