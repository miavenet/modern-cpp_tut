// terminate.cpp - a std::thread destroyed while still joinable.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/03-concurrency/lab/terminate.cpp -o "${TMPDIR:-/tmp}/terminate" && "${TMPDIR:-/tmp}/terminate"
#include <cstdio>
#include <thread>

int main() {
    {
        std::thread t([] { std::puts("worker ran"); });
        // no t.join() and no t.detach()
    }   // ~thread() runs here while t is still joinable
    std::puts("never printed");
}
