// map_tail.cpp -- module 07 lesson 05 (LPA lens): std::map vs std::flat_map, looked at
// as a DISTRIBUTION, not as one mean.
// Build and run (from the tutorial root):
//   clang++ -std=c++23 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/07-architecture/lab/bench/map_tail.cpp -o "${TMPDIR:-/tmp}/map_tail" && "${TMPDIR:-/tmp}/map_tail"
//
// A price table of 1,000,000 instruments (keys inserted in random order, the way a
// table fills during a day). Two views of the same lookups:
//   1. mini_bench style: 5 repetitions of 1,000,000 lookups, mean ns per lookup,
//      report the median of the 5 means (the one number lesson 03 printed)
//   2. per-lookup histogram: 1,000,000 lookups, each timed alone
// Both lookup views put the same compiler barrier (escape) after every lookup, so the
// two differ only in the clock reads: one pair per 1,000,000 lookups, or one per lookup.
// Then the price flat_map pays for that: 1,000 inserts of NEW keys, each timed (an
// insert changes the map, so it needs no barrier).
#include "lat_hist.hpp"

#include <algorithm>
#include <cstdio>
#include <flat_map>
#include <map>
#include <random>
#include <vector>

template <class T>
inline void escape(T& value) { asm volatile("" : "+r,m"(value) : : "memory"); }

constexpr std::size_t kKeys = 1'000'000;
constexpr std::size_t kLookups = 1'000'000;
constexpr int kInserts = 1'000;

template <class Map>
static double median_of_means(const Map& m, const std::vector<std::uint64_t>& probe) {
    std::vector<double> means;
    for (int rep = 0; rep < 5; ++rep) {
        double sum = 0;
        const std::uint64_t t0 = lat::now_ns();
        for (std::uint64_t k : probe) { sum += m.find(k)->second; escape(sum); }
        means.push_back(static_cast<double>(lat::now_ns() - t0) / probe.size());
    }
    std::sort(means.begin(), means.end());
    return means[2];
}

template <class Map>
static void per_lookup(const char* label, const Map& m, const std::vector<std::uint64_t>& probe,
                       std::uint64_t floor_ns) {
    static lat::histogram h;
    h.reset();
    double sum = 0;
    for (std::uint64_t k : probe) {
        const std::uint64_t t0 = lat::now_ns();
        sum += m.find(k)->second;
        escape(sum);
        h.record(lat::now_ns() - t0);
    }
    h.report(stdout, label, floor_ns);
}

template <class Map>
static void inserts(const char* label, Map& m, std::uint64_t floor_ns) {
    static lat::histogram h;
    h.reset();
    for (int i = 0; i < kInserts; ++i) {
        const std::uint64_t key = 2 * (static_cast<std::uint64_t>(i) * 997 % kKeys) + 1;  // odd: new
        const std::uint64_t t0 = lat::now_ns();
        m.emplace(key, 1.0);
        h.record(lat::now_ns() - t0);
    }
    h.report(stdout, label, floor_ns);
}

int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("%zu keys, %zu lookups; environment: clang %s, %s, one thread, no core pinning "
                "(macOS), shared machine\n", kKeys, kLookups, __clang_version__, opt);
    std::mt19937_64 rng(7);
    std::vector<std::uint64_t> keys(kKeys);
    for (std::size_t i = 0; i < kKeys; ++i) keys[i] = 2 * i;          // even keys exist
    std::shuffle(keys.begin(), keys.end(), rng);                      // random insertion order
    std::map<std::uint64_t, double> tree;
    for (std::uint64_t k : keys) tree.emplace(k, 1.0);
    // flat_map is built in one go from sorted keys; 1,000,000 single inserts would
    // be O(n^2) (see the insert row below for why).
    std::vector<std::uint64_t> sorted(keys);
    std::sort(sorted.begin(), sorted.end());
    std::flat_map<std::uint64_t, double> flat(std::sorted_unique, std::move(sorted),
                                              std::vector<double>(kKeys, 1.0));
    std::vector<std::uint64_t> probe(kLookups);
    for (auto& k : probe) k = keys[rng() % kKeys];

    std::printf("median of 5 means : map %.1f ns/lookup, flat_map %.1f ns/lookup\n",
                median_of_means(tree, probe), median_of_means(flat, probe));
    per_lookup("map lookup", tree, probe, floor_ns);
    per_lookup("flat lookup", flat, probe, floor_ns);
    inserts("map insert", tree, floor_ns);
    inserts("flat insert", flat, floor_ns);
}
