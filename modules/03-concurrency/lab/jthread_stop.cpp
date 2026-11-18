// jthread_stop.cpp - C++20 jthread: cooperative stop, automatic join.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/jthread_stop.cpp -o "${TMPDIR:-/tmp}/jthread_stop" && "${TMPDIR:-/tmp}/jthread_stop"
#include <chrono>
#include <cstdio>
#include <stop_token>
#include <thread>

int main() {
    int processed = 0;                          // written by the worker only
    {
        std::jthread worker([&processed](std::stop_token st) {
            while (!st.stop_requested()) {      // poll the stop flag
                ++processed;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }   // ~jthread(): request_stop(), then join()
    std::printf("worker stopped after %s items\n", processed > 0 ? "some" : "zero");
}
