// dangling.cpp - views that borrow vs views that own.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/dangling.cpp -o "${TMPDIR:-/tmp}/dangling" && "${TMPDIR:-/tmp}/dangling"
// To see the bug, add -DSHOW_BUG -g -fsanitize=address to the command above.
#include <algorithm>
#include <cstdio>
#include <ranges>
#include <vector>

auto pricesAbove100_borrowing() {
    std::vector<double> prices{100.0, 101.5, 99.0, 102.0};
    return prices | std::views::filter([](double p) { return p > 100; });
    // ref_view over the local 'prices': dangles after return
}

auto pricesAbove100_owning() {
    std::vector<double> prices{100.0, 101.5, 99.0, 102.0};
    return std::move(prices) | std::views::filter([](double p) { return p > 100; });
    // rvalue vector -> owning_view: the view now holds the vector
}

int main() {
    auto good = pricesAbove100_owning();
    for (double p : good) std::printf("%.1f ", p);
    std::printf("\n");
#ifdef SHOW_BUG
    auto bad = pricesAbove100_borrowing();
    for (double p : bad) std::printf("%.1f ", p);   // use after the vector died
    std::printf("\n");
#endif
}
