// lazy_views.cpp - views do work only when iterated; filter over a forward range caches begin().
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/lazy_views.cpp -o "${TMPDIR:-/tmp}/lazy_views" && "${TMPDIR:-/tmp}/lazy_views"
#include <cstdio>
#include <ranges>
#include <vector>

int main() {
    std::vector<int> nums{1, 2, 3, 4, 5, 6};
    int calls = 0;
    auto evens = nums | std::views::filter([&calls](int n) { ++calls; return n % 2 == 0; })
                      | std::views::transform([](int n) { return n * n; });
    std::printf("after building the pipeline: %d predicate calls\n", calls);

    for (int x : evens) std::printf("%d ", x);
    std::printf("\nafter first loop: %d predicate calls\n", calls);

    for (int x : evens) std::printf("%d ", x);
    std::printf("\nafter second loop: %d predicate calls\n", calls);
}
