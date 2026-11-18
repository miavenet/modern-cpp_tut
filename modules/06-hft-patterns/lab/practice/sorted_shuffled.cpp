// sorted_shuffled.cpp - same data, two orders: does the order change the time? Build with and without -DKEEP_BRANCH.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_a" && clang++ -std=c++20 -O2 -Wall -Wextra -DKEEP_BRANCH modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_b"
//   for r in 1 2 3; do for b in ss_a ss_b; do "${TMPDIR:-/tmp}/$b" sorted; "${TMPDIR:-/tmp}/$b" shuffled; done; done
//   clang++ -std=c++20 -O2 [-DKEEP_BRANCH] -S modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o -   (read sum_big)
// Same data, two orders. Does the order change the time? Build with and without -DKEEP_BRANCH.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
__attribute__((noinline)) long sum_big(const unsigned char* p, long n) {
    long s = 0;
    for (long i = 0; i < n; ++i) {
        if (p[i] >= 128) {
#ifdef KEEP_BRANCH
            asm volatile("");   // a side effect the compiler may not execute speculatively
#endif
            s += p[i];
        }
    }
    return s;
}
int main(int argc, char** argv) {
    bool sorted = argc > 1 && !std::strcmp(argv[1], "sorted");
    const long n = 1 << 20;
    std::vector<unsigned char> v(n);
    std::mt19937 rng(42);
    for (auto& x : v) x = static_cast<unsigned char>(rng() & 0xFF);
    if (sorted) std::sort(v.begin(), v.end());
    long total = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 200; ++rep) {
        asm volatile("" : : "r"(v.data()) : "memory");  // stop the compiler from reusing the last result
        total += sum_big(v.data(), n);
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / (200.0 * n);
    std::printf("order=%s ns_per_element=%.3f total=%ld\n", sorted ? "sorted" : "shuffled", ns, total);
}
