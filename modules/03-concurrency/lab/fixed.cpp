// fixed.cpp - the same program with std::atomic<int>: no data race.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/fixed.cpp -o "${TMPDIR:-/tmp}/fixed" && "${TMPDIR:-/tmp}/fixed"
#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> counter{0};

void work() {
    for (int i = 0; i < 100000; ++i)
        counter.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::thread a(work), b(work);
    a.join();
    b.join();
    std::cout << "counter = " << counter.load() << " (200000 expected)\n";
}
