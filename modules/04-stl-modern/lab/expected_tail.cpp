// expected_tail.cpp - LPA lens (lesson 05): WHERE in the latency distribution a throw lands.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++23 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/04-stl-modern/lab/expected_tail.cpp -o "${TMPDIR:-/tmp}/expected_tail" && "${TMPDIR:-/tmp}/expected_tail"
// The same order-check runs 1,000,000 times at three error rates (0, 0.01%, 1%),
// once reporting failure by throwing and once by returning std::expected.
// Bad orders sit at fixed pseudo-random positions (same seed every run), so the
// error COUNT is exact; only the times move. Each call is timed on its own, so
// anything under the ~42 ns clock tick reads as 0 or 42: watch the tail columns.
#include "lat_hist.hpp"

#include <cstdio>
#include <cstdlib>
#include <expected>
#include <new>
#include <stdexcept>
#include <vector>

static std::size_t g_news = 0;   // global operator new calls (the exception's message lives on the heap)
void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

enum class reject { bad_qty };

__attribute__((noinline)) int check_throw(int qty) {
    if (qty <= 0) throw std::invalid_argument("bad qty");
    return qty;
}
__attribute__((noinline)) std::expected<int, reject> check_expected(int qty) noexcept {
    if (qty <= 0) return std::unexpected(reject::bad_qty);
    return qty;
}

constexpr int kCalls = 1'000'000;

// qty[i] <= 0 marks a bad order. A fixed LCG puts exactly round(rate * kCalls) of them
// at pseudo-random positions, so every run sees the same pattern.
static std::vector<int> make_orders(int bad_per_million) {
    std::vector<int> qty(kCalls, 100);
    std::uint64_t x = 12345;
    int placed = 0;
    while (placed < bad_per_million) {
        x = x * 6364136223846793005ull + 1442695040888963407ull;
        const std::size_t i = (x >> 33) % kCalls;
        if (qty[i] > 0) { qty[i] = 0; ++placed; }
    }
    return qty;
}

struct row { long errors; std::size_t news; double mean_ns; };

template <bool Throw>
static row run(const std::vector<int>& qty, lat::histogram& h) {
    h.reset();
    long sum = 0, errors = 0;
    const std::size_t n0 = g_news;
    for (int i = 0; i < kCalls; ++i) {                 // timed per call
        const std::uint64_t t0 = lat::now_ns();
        if constexpr (Throw) {
            try { sum += check_throw(qty[i]); } catch (const std::invalid_argument&) { ++errors; }
        } else {
            auto r = check_expected(qty[i]);
            if (r) sum += *r; else ++errors;
        }
        h.record(lat::now_ns() - t0);
    }
    const std::size_t news = g_news - n0;
    // Second pass, a different measurement: one clock pair around all calls (no per-call
    // clock reads), so the mean is NOT the average of the timed samples above.
    const std::uint64_t t0 = lat::now_ns();
    for (int i = 0; i < kCalls; ++i) {
        if constexpr (Throw) {
            try { sum += check_throw(qty[i]); } catch (const std::invalid_argument&) { --sum; }
        } else {
            auto r = check_expected(qty[i]);
            sum += r ? *r : -1;
        }
    }
    const double mean = static_cast<double>(lat::now_ns() - t0) / kCalls;
    if (sum == 42) std::puts("");                      // keep sum observable
    return {errors, news, mean};
}

int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("order check x %d calls, each timed alone; environment: clang %s, %s, one thread, "
                "no core pinning (macOS), shared machine\n", kCalls, __clang_version__, opt);
    static lat::histogram h;
    const int rates[] = {0, 100, 10'000};               // bad orders per million: 0, 0.01%, 1%
    const char* names[] = {"0%", "0.01%", "1%"};
    for (int k = 0; k < 3; ++k) {
        const auto qty = make_orders(rates[k]);
        char label[32];
        const row t = run<true>(qty, h);
        std::snprintf(label, sizeof label, "throw %s", names[k]);
        h.report(stdout, label, floor_ns);
        std::printf("               errors = %ld, heap calls while timed = %zu, mean %.1f ns/call\n",
                    t.errors, t.news, t.mean_ns);
        const row e = run<false>(qty, h);
        std::snprintf(label, sizeof label, "expected %s", names[k]);
        h.report(stdout, label, floor_ns);
        std::printf("               errors = %ld, heap calls while timed = %zu, mean %.1f ns/call\n",
                    e.errors, e.news, e.mean_ns);
        std::fflush(stdout);
    }
}
