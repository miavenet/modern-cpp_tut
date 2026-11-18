// custom_range.cpp - make your own type work with std::ranges.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/custom_range.cpp -o "${TMPDIR:-/tmp}/custom_range" && "${TMPDIR:-/tmp}/custom_range"
#include <algorithm>
#include <cstdio>
#include <forward_list>
#include <ranges>
#include <vector>

class OrderBook {                                  // wraps a vector of prices
    std::vector<int> prices_{105, 101, 103};
public:
    auto begin() { return prices_.begin(); }        // member begin/end:
    auto end()   { return prices_.end(); }          // ranges::begin/end find them
};

static_assert(std::ranges::random_access_range<OrderBook>);

int main() {
    OrderBook book;
    std::ranges::sort(book);                        // needs random access
    for (int p : book | std::views::reverse) std::printf("%d ", p); // needs bidirectional
    std::printf("\n");
    static_assert(std::ranges::forward_range<std::forward_list<int>>);
    static_assert(!std::ranges::bidirectional_range<std::forward_list<int>>);
}
