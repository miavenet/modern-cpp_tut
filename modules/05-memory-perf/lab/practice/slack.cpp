// slack.cpp - branch-free "keep the odd order ids" with an output buffer sized to the exact kept count.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/slack.cpp -o "${TMPDIR:-/tmp}/slack" && "${TMPDIR:-/tmp}/slack" b && "${TMPDIR:-/tmp}/slack" b k && "${TMPDIR:-/tmp}/slack" n k && "${TMPDIR:-/tmp}/slack" n
// Branch-free "keep the odd order ids" with an output buffer sized to the exact kept count.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

std::size_t keep_odd_branchy(const std::uint32_t* in, std::size_t n, std::uint32_t* out) {
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] & 1u) out[m++] = in[i];
    return m;
}

std::size_t keep_odd_nobranch(const std::uint32_t* in, std::size_t n, std::uint32_t* out) {
    std::size_t m = 0;
    for (std::size_t i = 0; i < n; ++i) {
        out[m] = in[i];          // written every time, kept or not
        m += in[i] & 1u;
    }
    return m;
}

int main(int argc, char** argv) {
    bool nobranch = argc > 1 && argv[1][0] == 'n';
    bool tail_kept = argc > 2 && argv[2][0] == 'k';
    // 4 odd ids either way; only the order differs
    std::vector<std::uint32_t> ids = tail_kept ? std::vector<std::uint32_t>{20, 22, 11, 13, 15, 17}
                                               : std::vector<std::uint32_t>{11, 13, 15, 17, 20, 22};
    std::size_t kept = 0;
    for (std::uint32_t v : ids) kept += v & 1u;
    std::vector<std::uint32_t> out(kept);                         // exactly 4 slots
    std::size_t m = nobranch ? keep_odd_nobranch(ids.data(), ids.size(), out.data())
                             : keep_odd_branchy(ids.data(), ids.size(), out.data());
    std::printf("%s, last input %s: kept %zu of %zu, out capacity %zu\n",
                nobranch ? "nobranch" : "branchy", tail_kept ? "kept" : "rejected",
                m, ids.size(), out.size());
    return 0;
}
