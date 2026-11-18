// use_bad.cpp -- tries to NAME std::vector, which this file never included.
// Meant to FAIL. After building book.pcm as shown in use_ok.cpp, from the tutorial root:
//   T="${TMPDIR:-/tmp}"; M=modules/07-architecture/lab/modules
//   clang++ -std=c++20 -fcxx-modules "$M/use_bad.cpp" -fmodule-file=book="$T/book.pcm" -fsyntax-only
import book;

int main() {
    std::vector<int> w;     // error: the name std::vector is not visible in this file
    return 0;
}
