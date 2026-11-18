// tier_table.cpp - "decide earlier, look up later": a discount tier per order-size bucket, as a rule and as a constexpr-built table.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/tier_table.cpp -o "${TMPDIR:-/tmp}/tier_table" && "${TMPDIR:-/tmp}/tier_table"
//   bash modules/02-type-system/lab/practice/tier_listing.sh clang++   (the -O2 listing of by_rule and by_table)
// "Decide earlier, look up later": a discount tier per order-size bucket.
// rule():   the decision logic (a ladder of comparisons).
// TABLE:    the same answers, computed before the hot path runs.
#include <array>
#include <cstdint>
#include <cstdio>

struct Limits { std::uint8_t small, medium, large; };

constexpr std::uint8_t rule(std::uint8_t bucket, Limits lim) {
    if (bucket < lim.small)  return 0;
    if (bucket < lim.medium) return 3;
    if (bucket < lim.large)  return 7;
    return 12;
}

constexpr std::array<std::uint8_t, 256> build(Limits lim) {
    std::array<std::uint8_t, 256> t{};
    for (int b = 0; b < 256; ++b) t[static_cast<std::size_t>(b)] = rule(static_cast<std::uint8_t>(b), lim);
    return t;
}

constexpr Limits BOOT{10, 50, 200};
constexpr auto FIXED = build(BOOT);                 // built by the compiler
static_assert(FIXED[9] == 0 && FIXED[10] == 3 && FIXED[199] == 7 && FIXED[200] == 12);

// hot-path candidates (noinline so each has its own listing)
[[gnu::noinline]] std::uint8_t by_rule(std::uint8_t bucket, const Limits& lim) { return rule(bucket, lim); }
[[gnu::noinline]] std::uint8_t by_table(std::uint8_t bucket, const std::array<std::uint8_t, 256>& t) { return t[bucket]; }

int main() {
    // 1. table and rule agree on every input
    int disagree = 0;
    for (int b = 0; b < 256; ++b)
        if (by_table(static_cast<std::uint8_t>(b), FIXED) != by_rule(static_cast<std::uint8_t>(b), BOOT)) ++disagree;
    std::printf("inputs where table and rule disagree (same limits): %d of 256\n", disagree);

    // 2. the limits change at run time; the table has not been rebuilt yet
    Limits now{10, 80, 200};
    std::array<std::uint8_t, 256> live = FIXED;
    int stale = 0;
    for (int b = 0; b < 256; ++b)
        if (by_table(static_cast<std::uint8_t>(b), live) != by_rule(static_cast<std::uint8_t>(b), now)) ++stale;
    std::printf("after the limits change, before a rebuild: %d of 256 answers are stale\n", stale);

    // 3. rebuild off the hot path
    live = build(now);
    stale = 0;
    for (int b = 0; b < 256; ++b)
        if (by_table(static_cast<std::uint8_t>(b), live) != by_rule(static_cast<std::uint8_t>(b), now)) ++stale;
    std::printf("after the rebuild: %d of 256 answers are stale\n", stale);
    return 0;
}
