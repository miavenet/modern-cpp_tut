// main.cpp -- imports the module math_core.
// Two-step build with Apple clang (tested), from the tutorial root:
//   T="${TMPDIR:-/tmp}"; M=modules/07-architecture/lab/modules
//   clang++ -std=c++20 -fcxx-modules --precompile "$M/math_core.cppm" -o "$T/math_core.pcm"
//   clang++ -std=c++20 -fcxx-modules "$M/main.cpp" -fmodule-file=math_core="$T/math_core.pcm" \
//     "$T/math_core.pcm" -o "$T/mod_app" && "$T/mod_app"
import math_core;
#include <cstdio>

int main() {
    std::printf("add(2, 3) = %d\n", add(2, 3));
    hello();
#ifdef SECRET_MACRO
    std::puts("SECRET_MACRO leaked");
#else
    std::puts("SECRET_MACRO not visible here");
#endif
    // helper();   // error: 'helper' is not exported
}
