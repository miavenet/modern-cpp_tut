// pgo.cpp -- let a profile, not a guess, decide which path is common.
// From the tutorial root. Tested with Apple clang on macOS, where xcrun finds llvm-profdata:
//   T="${TMPDIR:-/tmp}"; F=modules/06-hft-patterns/lab/measure/pgo.cpp
//   clang++ -std=c++20 -O2 -fprofile-instr-generate "$F" -o "$T/pgo_gen"
//   LLVM_PROFILE_FILE="$T/pgo.profraw" "$T/pgo_gen"
//   xcrun llvm-profdata merge -o "$T/pgo.profdata" "$T/pgo.profraw"
//   clang++ -std=c++20 -O2 -fprofile-instr-use="$T/pgo.profdata" -S -o - "$F" | grep -A10 '^__Z6handleib:'
//   clang++ -std=c++20 -O2 -S -o - "$F" | grep -A10 '^__Z6handleib:'          # no profile, to compare
#include <cstdio>
[[gnu::noinline]] void on_halt() { std::puts("halt"); }
[[gnu::noinline]] int price_fast(int px) { return px + 1; }

[[gnu::noinline]] int handle(int px, bool halted) {
    if (halted) { on_halt(); return 0; }
    return price_fast(px);
}

int main(int argc, char**) {
    long sum = 0;
    for (int i = 0; i < 1'000'000; ++i) sum += handle(i, argc > 1 && i == 500'000);
    std::printf("sum=%ld\n", sum);
}
