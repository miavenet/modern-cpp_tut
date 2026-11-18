// calib_main.cpp - compare the full outputs of calib_stop and calib_all on clean and faulty input.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/calib.cpp modules/05-memory-perf/lab/practice/calib_main.cpp -o "${TMPDIR:-/tmp}/calib_main" && "${TMPDIR:-/tmp}/calib_main"
// Equivalence gate for calib_stop (A) and calib_all (B) from calib.cpp.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

constexpr std::int32_t SENTINEL = INT32_MIN;
std::size_t calib_stop(const std::int32_t*, std::int32_t*, std::size_t, std::int32_t);
std::size_t calib_all(const std::int32_t*, std::int32_t*, std::size_t, std::int32_t);

static void compare(const char* label, const std::vector<std::int32_t>& in) {
    const std::size_t n = in.size();
    std::vector<std::int32_t> a(n, -1), b(n, -1);   // -1 marks "never written"
    std::size_t ra = calib_stop(in.data(), a.data(), n, 7);
    std::size_t rb = calib_all(in.data(), b.data(), n, 7);
    std::size_t differ = 0, first = n;
    for (std::size_t i = 0; i < n; ++i)
        if (a[i] != b[i]) { if (differ == 0) first = i; ++differ; }
    std::printf("%-22s return A=%zu B=%zu  differing output elements=%zu", label, ra, rb, differ);
    if (differ) std::printf("  first at index %zu", first);
    std::printf("\n");
}

int main() {
    const std::size_t n = 1000;
    std::vector<std::int32_t> in(n);
    for (std::size_t i = 0; i < n; ++i) in[i] = static_cast<std::int32_t>(i % 97);
    compare("no fault", in);
    in[400] = SENTINEL;
    compare("fault at 400", in);
    in[900] = SENTINEL;
    compare("faults at 400 and 900", in);
    return 0;
}
