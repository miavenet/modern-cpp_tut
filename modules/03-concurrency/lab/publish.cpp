// publish.cpp - one thread publishes a plain struct, another reads it.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/publish.cpp -o "${TMPDIR:-/tmp}/publish" && "${TMPDIR:-/tmp}/publish"
#include <atomic>
#include <iostream>
#include <thread>

struct Quote { int bid; int ask; };

Quote quote;                       // plain (non-atomic) payload
std::atomic<bool> ready{false};    // the flag that publishes it

int main() {
    std::thread producer([] {
        quote = {100, 101};                              // (1) write payload
        ready.store(true, std::memory_order_release);    // (2) publish
    });
    std::thread consumer([] {
        while (!ready.load(std::memory_order_acquire)) {} // (3) wait for flag
        std::cout << quote.bid << ' ' << quote.ask << '\n'; // (4) read payload
    });
    producer.join();
    consumer.join();
}
