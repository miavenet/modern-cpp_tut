// affine_schedule.cpp - three fixed affine steps composed into one prepared pair; counts helper calls with and without preparation.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/affine_schedule.cpp -o "${TMPDIR:-/tmp}/affine_schedule" && "${TMPDIR:-/tmp}/affine_schedule"
#include <array>
#include <cstdint>
#include <cstdio>
struct Count {
    unsigned mul = 0, add = 0;
    std::int64_t multiply(std::int64_t a, std::int64_t b) { ++mul; return a*b; }
    std::int64_t plus(std::int64_t a, std::int64_t b) { ++add; return a+b; }
};
struct Affine { std::int64_t a, b; };
std::int64_t apply(Affine t, std::int64_t x, Count& c) {
    return c.plus(c.multiply(t.a,x),t.b);
}
Affine append(Affine previous, Affine next, Count& c) {
    return {c.multiply(next.a,previous.a), c.plus(c.multiply(next.a,previous.b),next.b)};
}
int main() {
    // Small signed inputs and coefficients; all intermediates fit int64_t.
    const std::array<Affine,3> stages{{{3,2},{-2,5},{4,-3}}};
    Count setup;
    const Affine folded = append(append(stages[0],stages[1],setup),stages[2],setup);
    std::printf("folded a=%lld b=%lld setup_mul=%u setup_add=%u\n",
                static_cast<long long>(folded.a),static_cast<long long>(folded.b),setup.mul,setup.add);
    Count direct, ready;
    unsigned mismatches=0;
    for (std::int64_t x : {-3,0,7}) {
        auto y=x;
        for (auto t:stages) y=apply(t,y,direct);
        const auto z=apply(folded,x,ready);
        mismatches += y!=z;
        std::printf("x=%lld direct=%lld prepared=%lld\n",
                    static_cast<long long>(x),static_cast<long long>(y),static_cast<long long>(z));
    }
    std::printf("samples=3 direct_mul=%u direct_add=%u prepared_mul=%u prepared_add=%u mismatches=%u\n",
                direct.mul,direct.add,ready.mul,ready.add,mismatches);
    return mismatches!=0;
}
