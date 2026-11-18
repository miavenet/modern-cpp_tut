// async_block.cpp - a discarded launch::async future waits when it is the last reference.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/03-concurrency/lab/async_block.cpp -o "${TMPDIR:-/tmp}/async_block" && "${TMPDIR:-/tmp}/async_block"
#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

using namespace std::chrono;

void slow() { std::this_thread::sleep_for(milliseconds(200)); }

int main() {
    auto t0 = steady_clock::now();
    std::async(std::launch::async, slow);   // future discarded at the ";" (clang warns: nodiscard)
    auto ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    std::printf("statement took %s 200 ms\n", ms >= 200 ? "at least" : "less than");

    t0 = steady_clock::now();
    auto f = std::async(std::launch::async, slow);  // future kept alive
    ms = duration_cast<milliseconds>(steady_clock::now() - t0).count();
    std::printf("statement took %s 200 ms\n", ms >= 200 ? "at least" : "less than");
    f.get();                                // wait here instead
}
