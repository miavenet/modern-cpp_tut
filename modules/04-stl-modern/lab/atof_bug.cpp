// atof_bug.cpp - a view is not a C string.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/atof_bug.cpp -o "${TMPDIR:-/tmp}/atof_bug" && "${TMPDIR:-/tmp}/atof_bug"
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main() {
    std::string msg = "121275";                 // fixed width: qty(2) then price(4)
    std::string_view qty = std::string_view(msg).substr(0, 2);   // "12"
    std::printf("view: '%.*s'\n", (int)qty.size(), qty.data());
    std::printf("atof(qty.data()) = %.0f\n", std::atof(qty.data()));  // reads to the '\0'
    int q = 0;
    std::from_chars(qty.data(), qty.data() + qty.size(), q);          // reads 2 chars
    std::printf("from_chars       = %d\n", q);
}
