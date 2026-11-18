// predictor_model.cpp - a software model of three simple branch predictors; it counts mispredictions exactly.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/practice/predictor_model.cpp -o "${TMPDIR:-/tmp}/predictor_model" && "${TMPDIR:-/tmp}/predictor_model"
// A software model of three very simple branch predictors.
// It counts mispredictions exactly, so the output is the same on every machine.
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

// (a) always guess the more common outcome
static std::uint64_t miss_majority(const std::vector<std::uint8_t>& t) {
    std::uint64_t taken = 0;
    for (std::uint8_t b : t) taken += b;
    return taken * 2 >= t.size() ? t.size() - taken : taken;
}

// (b) one 2-bit saturating counter: 0,1 predict not-taken; 2,3 predict taken
static std::uint64_t miss_counter(const std::vector<std::uint8_t>& t) {
    unsigned c = 1;
    std::uint64_t miss = 0;
    for (std::uint8_t b : t) {
        miss += ((c >= 2) != (b != 0));
        if (b) { if (c < 3) ++c; } else { if (c > 0) --c; }
    }
    return miss;
}

// (c) 2-bit counters indexed by the last H outcomes (a tiny history predictor)
static std::uint64_t miss_history(const std::vector<std::uint8_t>& t, unsigned H) {
    std::vector<std::uint8_t> table(std::size_t{1} << H, 1);
    std::uint32_t hist = 0, mask = (1u << H) - 1u;
    std::uint64_t miss = 0;
    for (std::uint8_t b : t) {
        std::uint8_t& c = table[hist];
        miss += ((c >= 2) != (b != 0));
        if (b) { if (c < 3) ++c; } else { if (c > 0) --c; }
        hist = ((hist << 1) | (b ? 1u : 0u)) & mask;
    }
    return miss;
}

static void report(const char* name, const std::vector<std::uint8_t>& t) {
    std::uint64_t taken = 0;
    for (std::uint8_t b : t) taken += b;
    double n = static_cast<double>(t.size());
    std::printf("%-22s taken=%.4f  majority=%.4f  counter2bit=%.4f  history8=%.4f\n", name,
                static_cast<double>(taken) / n,
                static_cast<double>(miss_majority(t)) / n,
                static_cast<double>(miss_counter(t)) / n,
                static_cast<double>(miss_history(t, 8)) / n);
}

int main() {
    const std::size_t N = 4'000'000;
    std::mt19937_64 rng(12345);                 // fixed seed; the engine's output is fixed by the standard
    for (unsigned pct : {50u, 40u, 25u, 15u, 2u}) {
        std::vector<std::uint8_t> t(N);
        const std::uint64_t cut = (~std::uint64_t{0} / 100u) * pct;
        for (auto& b : t) b = rng() < cut;      // independent outcomes, taken with probability pct/100
        char name[32];
        std::snprintf(name, sizeof name, "random p=%.2f", pct / 100.0);
        report(name, t);
    }
    std::vector<std::uint8_t> t(N);
    for (std::size_t i = 0; i < N; ++i) t[i] = (i % 4 == 0);   // T N N N T N N N ... also 25% taken
    report("repeating TNNN (25%)", t);
    return 0;
}
