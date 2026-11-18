// throw_cost.cpp - cost of reporting a failure by throwing vs by returning std::expected (C++23).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++23 -Wall -Wextra -O2 modules/04-stl-modern/lab/throw_cost.cpp -o "${TMPDIR:-/tmp}/throw_cost" && "${TMPDIR:-/tmp}/throw_cost"
// Half of the calls fail. Timings depend on your machine; run it twice.
#include <chrono>
#include <cstdio>
#include <expected>
#include <stdexcept>
__attribute__((noinline)) int parse_throw(int i) { if (i % 2) throw std::runtime_error("bad"); return i; }
__attribute__((noinline)) std::expected<int, int> parse_exp(int i) { if (i % 2) return std::unexpected(1); return i; }
int main() {
    using C = std::chrono::steady_clock; constexpr int N = 200000; long sum = 0;
    auto t0 = C::now();
    for (int i = 0; i < N; ++i) { try { sum += parse_throw(i); } catch (const std::exception&) { --sum; } }
    auto t1 = C::now();
    for (int i = 0; i < N; ++i) { auto r = parse_exp(i); sum += r ? *r : -1; }
    auto t2 = C::now();
    auto ns = [](auto d) { return (long long)std::chrono::duration_cast<std::chrono::nanoseconds>(d).count(); };
    std::printf("throw: %lld ns per call, expected: %lld ns per call (sum %ld)\n", ns(t1 - t0) / N, ns(t2 - t1) / N, sum);
}
