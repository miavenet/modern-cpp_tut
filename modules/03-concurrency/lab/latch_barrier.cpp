// latch_barrier.cpp - C++20 std::latch (one-shot) and std::barrier (reusable).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/latch_barrier.cpp -o "${TMPDIR:-/tmp}/latch_barrier" && "${TMPDIR:-/tmp}/latch_barrier"
#include <barrier>
#include <cstdio>
#include <latch>
#include <thread>
#include <vector>

int main() {
    constexpr int kWorkers = 3;
    int ready_flags[kWorkers] = {};             // each worker writes only its own slot

    std::latch started(kWorkers);               // counts down from 3, once
    int phase = 0;                              // touched only by the completion step
    std::barrier sync(kWorkers, [&phase]() noexcept {
        std::printf("phase %d complete\n", phase++);  // runs once per phase
    });

    std::vector<std::jthread> workers;
    for (int id = 0; id < kWorkers; ++id) {
        workers.emplace_back([&, id] {
            ready_flags[id] = 1;                // "initialise"
            started.count_down();               // tell main this worker is ready
            for (int round = 0; round < 2; ++round)
                sync.arrive_and_wait();         // wait for all 3, then reuse
        });
    }

    started.wait();                             // blocks until the count is 0
    int sum = ready_flags[0] + ready_flags[1] + ready_flags[2];
    std::printf("all %d workers initialised\n", sum);
}   // jthreads join here
