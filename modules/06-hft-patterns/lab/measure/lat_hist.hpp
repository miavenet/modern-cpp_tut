// lat_hist.hpp — shared latency histogram for the course's measured labs.
//
// One header, no dependencies beyond the standard library, C++20.
// Used by: modules/05-memory-perf/lab (pool tail lab); later LPA labs include it
// by path. Unit test: lab/measure/test_lat_hist.cpp (build line at its top).
//
// What it does
//   * record(ns) puts one sample into a fixed array of log-linear buckets
//     (HDR-style: 32 sub-buckets per power of two). No allocation, no locks,
//     no branches that depend on the sample count. Values 0..63 are exact;
//     above that a reported value is at most 1/32 (3.1%) above the true one.
//   * percentile(q) uses the nearest-rank rule: the smallest recorded value v
//     such that at least ceil(q * n) samples are <= v (reported as the top of
//     v's bucket, clamped to the true max). p100 is the exact max.
//   * Too few samples make a tail percentile noise. enough(q) requires at
//     least kTailSamples = 10 samples strictly above the percentile's rank,
//     i.e. n >= 10 / (1 - q): p99 needs 1,000, p99.9 needs 10,000,
//     p99.99 needs 100,000. report() prints "n/a" instead of a number below
//     that, and value_or_zero() is the only way to read one anyway.
//
// The clock floor (be honest about it)
//   On the course Mac (Apple M1 Max) steady_clock advances in steps of the
//   24 MHz system counter, about 41.7 ns. A single operation that takes 20 ns
//   reads as 0 or 42. So: time a BATCH of operations per sample and record the
//   batch's total; clock_floor_ns() measures the smallest nonzero step on this
//   machine and report() prints it next to every histogram. A histogram of
//   batches describes batch latency, not single-operation latency.
//
// This header measures; it does not judge. Course checks never pass or fail
// on a time. They grade structure and deterministic counters.

#pragma once

#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace lat {

inline constexpr std::uint64_t kTailSamples = 10;   // samples beyond the rank

// Nanoseconds from steady_clock. Two calls bracket the timed work.
inline std::uint64_t now_ns() noexcept {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

// Smallest nonzero difference between back-to-back clock reads (the tick).
inline std::uint64_t clock_floor_ns(int tries = 20000) noexcept {
    std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
    for (int i = 0; i < tries; ++i) {
        const std::uint64_t a = now_ns();
        std::uint64_t b = now_ns();
        while (b == a) b = now_ns();
        if (b - a < best) best = b - a;
    }
    return best;
}

// Minimum sample count for a trustworthy percentile q (0 < q < 1).
inline std::uint64_t min_samples(double q) noexcept {
    // The relative nudge absorbs rounding: 1 - 0.9999 is not exact in binary.
    const double need = static_cast<double>(kTailSamples) / (1.0 - q);
    return static_cast<std::uint64_t>(std::ceil(need * (1.0 - 1e-9)));
}

class histogram {
public:
    static constexpr unsigned kSubBits = 5;                       // 32 sub-buckets
    static constexpr std::uint64_t kSub = std::uint64_t{1} << kSubBits;
    static constexpr std::size_t kBuckets = 2 * kSub + (63 - kSubBits) * kSub;

    // Bucket index of a value. Monotonic in v; exact below 2*kSub.
    static constexpr std::size_t index(std::uint64_t v) noexcept {
        if (v < 2 * kSub) return static_cast<std::size_t>(v);
        const unsigned e = 63u - static_cast<unsigned>(std::countl_zero(v)); // e >= 6
        const unsigned shift = e - kSubBits;                                 // >= 1
        const std::uint64_t mant = (v >> shift) - kSub;                      // 0..31
        return static_cast<std::size_t>(2 * kSub + (e - kSubBits - 1) * kSub + mant);
    }
    // Largest value that lands in bucket i.
    static constexpr std::uint64_t upper(std::size_t i) noexcept {
        if (i < 2 * kSub) return i;
        const std::size_t k = i - 2 * kSub;
        const unsigned shift = static_cast<unsigned>(k / kSub) + 1;
        const std::uint64_t mant = kSub + (k % kSub);
        const std::uint64_t lo = mant << shift;
        return lo + ((std::uint64_t{1} << shift) - 1);
    }

    void record(std::uint64_t v) noexcept {
        ++counts_[index(v)];
        ++n_;
        if (v > max_) max_ = v;
        if (v < min_) min_ = v;
    }
    void reset() noexcept { counts_.fill(0); n_ = 0; max_ = 0; min_ = kNone; }

    std::uint64_t count() const noexcept { return n_; }
    std::uint64_t max() const noexcept { return n_ ? max_ : 0; }
    std::uint64_t min() const noexcept { return n_ ? min_ : 0; }

    // Is there enough data to report percentile q? (q == 1 means max: always.)
    bool enough(double q) const noexcept {
        return n_ > 0 && (q >= 1.0 || n_ >= min_samples(q));
    }

    // Nearest-rank percentile, 0 < q <= 1. Returns 0 when there are no samples.
    // Call enough(q) first: below the minimum this number is noise.
    std::uint64_t value_or_zero(double q) const noexcept {
        if (n_ == 0) return 0;
        if (q >= 1.0) return max_;
        // Nearest rank = ceil(q * n); the relative nudge absorbs binary rounding.
        std::uint64_t rank = static_cast<std::uint64_t>(
            std::ceil(q * static_cast<double>(n_) * (1.0 - 1e-12)));
        if (rank < 1) rank = 1;
        std::uint64_t seen = 0;
        for (std::size_t i = 0; i < kBuckets; ++i) {
            seen += counts_[i];
            if (seen >= rank) {
                const std::uint64_t u = upper(i);
                if (u > max_) return max_;
                return u < min_ ? min_ : u;
            }
        }
        return max_;
    }

    // One line: label, n, p50, p99, p99.9, p99.99, max, clock floor.
    void report(std::FILE* out, const char* label, std::uint64_t floor_ns) const {
        std::fprintf(out, "%-14s n=%llu", label, static_cast<unsigned long long>(n_));
        static constexpr double qs[] = {0.50, 0.99, 0.999, 0.9999};
        static constexpr const char* names[] = {"p50", "p99", "p99.9", "p99.99"};
        for (int i = 0; i < 4; ++i) {
            if (enough(qs[i]))
                std::fprintf(out, "  %s=%llu", names[i],
                             static_cast<unsigned long long>(value_or_zero(qs[i])));
            else
                std::fprintf(out, "  %s=n/a", names[i]);
        }
        std::fprintf(out, "  max=%llu ns  (clock floor %llu ns)\n",
                     static_cast<unsigned long long>(max()),
                     static_cast<unsigned long long>(floor_ns));
    }

private:
    static constexpr std::uint64_t kNone = std::numeric_limits<std::uint64_t>::max();
    std::array<std::uint64_t, kBuckets> counts_{};
    std::uint64_t n_ = 0, max_ = 0, min_ = kNone;
};

}  // namespace lat
