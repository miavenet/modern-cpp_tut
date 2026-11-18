// race.cpp -- a data race: two threads write `counter` with no synchronization.
// Plain:  clang++ -std=c++20 -O2 modules/07-architecture/lab/sanitizers/race.cpp -o "${TMPDIR:-/tmp}/race" && "${TMPDIR:-/tmp}/race"
// TSan:   clang++ -std=c++20 -O1 -g -fsanitize=thread modules/07-architecture/lab/sanitizers/race.cpp -o "${TMPDIR:-/tmp}/race_t" && "${TMPDIR:-/tmp}/race_t"
#include <cstdio>
#include <thread>

int counter = 0;

int main() {
    std::thread a([] { for (int i = 0; i < 100000; ++i) ++counter; });
    std::thread b([] { for (int i = 0; i < 100000; ++i) ++counter; });
    a.join();
    b.join();
    std::printf("counter = %d (200000 expected)\n", counter);
}
