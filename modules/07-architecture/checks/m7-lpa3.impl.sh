#!/bin/bash
# Check for M7-LPA3: report the tail a median hides. Learner's answer on stdin: one
# C++ expression giving the nearest-rank p99.99 of `s`, a SORTED std::vector<std::uint64_t>
# with s.size() >= 1. Nearest rank: the smallest sample with at least ceil(0.9999 * n)
# samples at or below it, i.e. s[ceil(0.9999 * n) - 1]. Graded on fixed inputs only.
set -u
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*;$//')
if [ -z "$answer" ]; then echo 'MISS: give one C++ expression'; exit 1; fi
case "$answer" in
    *';'*|*'{'*|*'}'*|*'#'*|*'\'*) echo 'MISS: give one expression, not statements'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'; exit 2
fi
cat >"$tmp/probe.cpp" <<CPP
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
static std::uint64_t p9999(const std::vector<std::uint64_t>& s) {
    using namespace std;
    return static_cast<std::uint64_t>($answer);
}
int main() {
    // s[i] = 100 * (i + 1) + 7: NOT the identity, so only a real index into s gives the
    // right sample; an expression that just computes the rank cannot match.
    const std::size_t ns[] = {1, 2, 10, 9'999, 10'000, 10'001, 20'000, 99'999, 100'000, 123'457, 1'000'000};
    for (std::size_t n : ns) {
        std::vector<std::uint64_t> s(n);
        for (std::size_t i = 0; i < n; ++i) s[i] = 100 * (i + 1) + 7;
        const std::uint64_t rank = (n * 9999 + 9999) / 10000;
        if (p9999(s) != s[rank - 1]) return 3;
    }
    return 0;
}
CPP
if ! clang++ -std=c++20 -O1 -fsanitize=address,undefined -fno-sanitize-recover=all "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that expression does not compile (s is a const std::vector<std::uint64_t>&)'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
"$tmp/probe" >"$tmp/run.log" 2>&1
case $? in
    0) echo 'HIT (nearest-rank p99.99 correct for 11 sample counts from 1 to 1,000,000)'; exit 0 ;;
    3) echo 'MISS: wrong sample for some n (check n = 10,000: the answer is s[9998]; watch floor vs ceil and floating-point rounding)'; exit 1 ;;
    *) echo 'MISS: the expression read outside the vector for some n (index must be at most n - 1)'; exit 1 ;;
esac
