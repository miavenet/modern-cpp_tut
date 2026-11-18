// fmt.cpp - std::format (C++20) and destination-bounded format_to_n.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/fmt.cpp -o "${TMPDIR:-/tmp}/fmt" && "${TMPDIR:-/tmp}/fmt"
#include <cstdio>
#include <format>
#include <string>

int main() {
    double price = 101.256; int qty = 300;
    std::string s = std::format("Price: {:.2f}, Qty: {}", price, qty);
    std::printf("%s (length %zu)\n", s.c_str(), s.size());

    char buf[16];                                       // fixed capacity
    auto r = std::format_to_n(buf, sizeof buf - 1, "Price: {:.2f}, Qty: {}", price, qty);
    *r.out = '\0';                                      // terminate what was written
    std::printf("buffer holds \"%s\"; full text needed %td chars\n", buf, r.size);
}
