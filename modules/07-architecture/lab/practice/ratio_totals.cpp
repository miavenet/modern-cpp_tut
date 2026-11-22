// ratio_totals.cpp - mean of per-file compression ratios versus the pooled ratio for two invented byte-count fixtures.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/ratio_totals.cpp -o "${TMPDIR:-/tmp}/ratio_totals" && "${TMPDIR:-/tmp}/ratio_totals"
#include <array>
#include <cstdio>
struct File { unsigned raw, stored; };
int main() {
    constexpr std::array<File, 2> files{{{900, 100}, {100, 100}}};
    unsigned raw=0, stored=0;
    double sum=0;
    for (auto f : files) { raw+=f.raw; stored+=f.stored; sum+=double(f.raw)/f.stored; }
    std::printf("mean_ratio=%.1f pooled_ratio=%.1f\n",sum/files.size(),double(raw)/stored);
    constexpr std::array<File, 2> other{{{900, 300}, {100, 10}}};
    raw=0; stored=0; sum=0;
    for (auto f : other) { raw+=f.raw; stored+=f.stored; sum+=double(f.raw)/f.stored; }
    std::printf("mean_ratio=%.1f pooled_ratio=%.6f raw=%u stored=%u\n",sum/other.size(),double(raw)/stored,raw,stored);
}
