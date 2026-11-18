// pack.cpp - three loops over a byte buffer; which does the auto-vectorizer accept?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
//   g++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -fopt-info-vec-optimized -fopt-info-vec-missed   (GCC)
// Three loops over a byte buffer. Which ones does the auto-vectorizer accept?
#include <cstddef>

// Same index in and out: input byte i always lands in output byte i.
void upper(const unsigned char* in, unsigned char* out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char c = in[i];
        out[i] = (c >= 'a' && c <= 'z') ? static_cast<unsigned char>(c - 32) : c;
    }
}

// Compaction: where byte i lands depends on how many earlier bytes were kept.
std::size_t strip_spaces(const unsigned char* in, unsigned char* out, std::size_t n) {
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] != ' ') out[m++] = in[i];
    return m;
}

// Same compaction, written without a branch.
std::size_t strip_spaces_nobranch(const unsigned char* in, unsigned char* out, std::size_t n) {
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i) {
        out[m] = in[i];
        m += (in[i] != ' ');
    }
    return m;
}
