#!/bin/bash
# Check for M7-LPA6: finish the nearest-rank p99.99 index (completion).
# Learner's answer on stdin: the two blanks of
#   s[(s.size() * ___ + ___) / 10000 - 1]
# separated by a comma. The filled expression is built and run on fixed inputs
# against s[ceil(0.9999 * n) - 1] (n = 1 to 1,000,000, including 10,000 and 10,001).
set -u
answer=$(head -n 1 | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
IFS=, read -r b1 b2 extra <<< "$answer"
b1=$(printf '%s' "${b1:-}" | tr -d '[:space:]'); b2=$(printf '%s' "${b2:-}" | tr -d '[:space:]')
if [ -n "${extra:-}" ] || ! [[ "$b1" =~ ^[0-9]+$ && "$b2" =~ ^[0-9]+$ ]]; then
    echo 'MISS: answer two whole numbers separated by a comma, in blank order'; exit 1
fi
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat >"$tmp/probe.cpp" <<CPP
#include <cstddef>
#include <cstdint>
#include <vector>
static std::uint64_t p9999(const std::vector<std::uint64_t>& s) {
    return s[(s.size() * ${b1}ULL + ${b2}ULL) / 10000 - 1];
}
int main() {
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
    echo 'SETUP PROBLEM: the filled-in expression does not build'; head -3 "$tmp/build.log"; exit 2
fi
"$tmp/probe" >"$tmp/run.log" 2>&1
case $? in
    0) echo 'HIT (nearest-rank p99.99 correct for 11 sample counts from 1 to 1,000,000)'; exit 0 ;;
    3) echo 'MISS: wrong sample for some n; the multiplier is 10000 - 1 and the added blank makes the division round UP (it is the divisor minus 1); check n = 10,000: the answer is s[9998]'; exit 1 ;;
    *) echo 'MISS: the expression read outside the vector (n = 1 is the edge case)'; exit 1 ;;
esac
