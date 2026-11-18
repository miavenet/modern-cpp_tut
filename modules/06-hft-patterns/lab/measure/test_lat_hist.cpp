// Unit test for lat_hist.hpp: known distributions -> known percentiles.
//
// Build and run from the tutorial root:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/06-hft-patterns/lab/measure/test_lat_hist.cpp -o "${TMPDIR:-/tmp}/test_lat_hist" && "${TMPDIR:-/tmp}/test_lat_hist"
//
// Prints one line per failed expectation and "lat_hist: ALL PASS" (exit 0) or
// "lat_hist: N FAILED" (exit 1). No test here depends on how fast anything ran.

#include "lat_hist.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

static int failures = 0;
#define EXPECT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            ++failures;                                                       \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);               \
        }                                                                     \
    } while (0)

// Count global allocations so we can prove record() never allocates.
static std::size_t g_news = 0;
void* operator new(std::size_t n) {
    ++g_news;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static lat::histogram h;   // static storage: the array is ~15 KB

static bool within(std::uint64_t got, std::uint64_t truth) {
    // Reported value is never below the truth and at most 1/32 above it.
    return got >= truth && got <= truth + truth / 32;
}

int main() {
    using lat::histogram;

    // 1. Bucket mapping: monotonic, exact below 64, every value inside its bucket.
    for (std::uint64_t v = 0; v < 64; ++v) EXPECT(histogram::upper(histogram::index(v)) == v);
    {
        bool ok = true;
        std::size_t prev = 0;
        for (std::uint64_t v = 1; v < (1u << 20); ++v) {
            const std::size_t i = histogram::index(v);
            if (i < prev || histogram::upper(i) < v ||
                histogram::upper(i) - v > v / 32) ok = false;
            prev = i;
        }
        EXPECT(ok);
        EXPECT(histogram::index(UINT64_MAX) == histogram::kBuckets - 1);
        EXPECT(histogram::upper(histogram::kBuckets - 1) == UINT64_MAX);
    }

    // 2. Exact small distribution: 1..100 once each (nearest rank).
    h.reset();
    for (std::uint64_t v = 1; v <= 100; ++v) h.record(v);
    EXPECT(h.count() == 100);
    EXPECT(h.value_or_zero(0.50) == 50);
    EXPECT(h.value_or_zero(0.99) == 99);
    EXPECT(h.value_or_zero(0.999) == 100);   // rank ceil(99.9) = 100
    EXPECT(h.value_or_zero(1.0) == 100);
    EXPECT(h.min() == 1 && h.max() == 100);

    // 3. Uniform 1..1,000,000: percentiles within the 1/32 bucket bound.
    h.reset();
    for (std::uint64_t v = 1; v <= 1'000'000; ++v) h.record(v);
    EXPECT(within(h.value_or_zero(0.50), 500'000));
    EXPECT(within(h.value_or_zero(0.99), 990'000));
    EXPECT(within(h.value_or_zero(0.999), 999'000));
    EXPECT(within(h.value_or_zero(0.9999), 999'900));
    EXPECT(h.max() == 1'000'000);

    // 4. A rare slow path: 1 sample in 1,000 costs 10,000x more. The tail
    //    percentiles above the rare fraction see it; the ones below do not.
    //    (50 is below 64, so it is recorded exactly.)
    h.reset();
    for (int i = 0; i < 1'000'000; ++i) h.record(i % 1000 == 999 ? 1'000'000 : 50);
    EXPECT(h.value_or_zero(0.50) == 50);
    EXPECT(h.value_or_zero(0.99) == 50);
    EXPECT(h.value_or_zero(0.999) == 50);           // rank 999,000: still fast
    EXPECT(within(h.value_or_zero(0.9999), 1'000'000));
    EXPECT(h.max() == 1'000'000);

    // 5. Sample-count rule: p99.99 needs >= 100,000 samples, p99.9 >= 10,000.
    EXPECT(lat::min_samples(0.99) == 1'000);
    EXPECT(lat::min_samples(0.999) == 10'000);
    EXPECT(lat::min_samples(0.9999) == 100'000);
    h.reset();
    for (int i = 0; i < 99'999; ++i) h.record(5);
    EXPECT(h.enough(0.999) && !h.enough(0.9999) && h.enough(1.0));
    h.record(5);
    EXPECT(h.enough(0.9999));
    h.reset();
    EXPECT(!h.enough(0.5) && h.value_or_zero(0.5) == 0 && h.max() == 0 && h.min() == 0);

    // 6. Extremes: zero and the largest value are recorded, not lost.
    h.reset();
    h.record(0);
    h.record(UINT64_MAX);
    EXPECT(h.count() == 2 && h.min() == 0 && h.max() == UINT64_MAX);
    EXPECT(h.value_or_zero(0.5) == 0);

    // 7. record() makes no heap call (the array is fixed-size).
    const std::size_t before = g_news;
    for (std::uint64_t v = 0; v < 100'000; ++v) h.record(v * 7919);
    EXPECT(g_news == before);

    // 8. The clock floor is positive (the tick is not zero).
    EXPECT(lat::clock_floor_ns(200) > 0);

    if (failures) {
        std::printf("lat_hist: %d FAILED\n", failures);
        return 1;
    }
    std::puts("lat_hist: ALL PASS");
    return 0;
}
