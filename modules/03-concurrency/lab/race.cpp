// race.cpp - two threads increment one plain int: a data race.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/race.cpp -o "${TMPDIR:-/tmp}/race" && "${TMPDIR:-/tmp}/race"
#include <iostream>
#include <thread>

int counter = 0;                       // plain int, shared, no protection

void work() {
    for (int i = 0; i < 100000; ++i) ++counter;   // read, add, write
}

int main() {
    std::thread a(work), b(work);
    a.join();
    b.join();
    std::cout << "counter = " << counter << " (200000 expected)\n";
}
