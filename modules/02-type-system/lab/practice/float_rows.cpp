// float_rows.cpp - 32-bit row numbers stored in a float: which survive the round trip?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/float_rows.cpp -o "${TMPDIR:-/tmp}/float_rows" && "${TMPDIR:-/tmp}/float_rows"
// 32-bit row numbers stored in a float: which survive the round trip?
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <initializer_list>

int main(int argc, char** argv) {
    const std::int32_t rows[] = {16777215, 16777216, 16777217, 16777219, 1073741825};
    for (std::int32_t row : rows) {
        float f = static_cast<float>(row);
        long back = static_cast<long>(f);
        std::printf("row %11d -> float -> %11ld  (error %+ld)\n", row, back, back - row);
    }
    for (float x : {16777216.0f, 1073741824.0f}) {
        std::printf("gap to next float above %.0f = %.0f\n", x, std::nextafter(x, INFINITY) - x);
    }
    std::printf("FLT_MANT_DIG-style digits: float %d, double %d\n",
                std::numeric_limits<float>::digits, std::numeric_limits<double>::digits);
    if (argc > 1 && std::strcmp(argv[1], "big") == 0) {
        volatile float big = 3.0e9f;               // above INT32_MAX
        std::int32_t i = static_cast<std::int32_t>(big); // UB per [conv.fpint]/1
        std::printf("cast result printed anyway: %d\n", i);
    }
}
