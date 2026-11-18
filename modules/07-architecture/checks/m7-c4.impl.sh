#!/bin/bash
# Check for M7-C4: benchmark::DoNotOptimize completion (compile-verified)
# Learner's answer on stdin. Blank: DoNotOptimize. To keep this check
# independent of the optional Google Benchmark installation, compile against
# a minimal stub that only defines DoNotOptimize. This is not a performance test.
answer=$(cat)

if [[ ! "$answer" =~ ^[[:space:]]*DoNotOptimize[[:space:]]*$ ]]; then
    echo "MISS: answer must be exactly DoNotOptimize"; exit 1
fi
b1=DoNotOptimize

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <string>
namespace benchmark {
    template <class T> void DoNotOptimize(T&& value) { (void)value; }
}
int main() {
    std::string empty_string;
    benchmark::$b1(empty_string);
}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
