// use_ok.cpp -- uses the returned vector without ever naming its type.
// Build from the tutorial root (Apple clang, tested):
//   T="${TMPDIR:-/tmp}"; M=modules/07-architecture/lab/modules
//   clang++ -std=c++20 -fcxx-modules --precompile "$M/book.cppm" -o "$T/book.pcm"
//   clang++ -std=c++20 -fcxx-modules "$M/use_ok.cpp" -fmodule-file=book="$T/book.pcm" "$T/book.pcm" -o "$T/use_ok" && "$T/use_ok"
import book;
#include <cstdio>

int main() {
    auto v = best_prices();     // the type is reachable through the exported function
    std::printf("%zu levels, best %d\n", v.size(), v[0]);
}
