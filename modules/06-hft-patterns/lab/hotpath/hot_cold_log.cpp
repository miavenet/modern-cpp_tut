// hot_cold_log.cpp -- the hot thread records a fixed-size event; a cold thread formats it.
// Run from the tutorial root:
//   clang++ -std=c++20 -O2 -I modules/06-hft-patterns/lab/spsc/solution \
//     modules/06-hft-patterns/lab/hotpath/hot_cold_log.cpp -o "${TMPDIR:-/tmp}/hot_cold" && "${TMPDIR:-/tmp}/hot_cold"
#include "spsc_ring.hpp"            // the lesson 01 ring (found with -I .../lab/spsc/solution)
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

struct SentEvent { std::uint64_t order_id; std::int64_t price_ticks; };   // 16 bytes, no strings

SpscRing<SentEvent, 4096> log_ring;
std::atomic<bool> done{false};

void hot_path_send(std::uint64_t id, std::int64_t px) {
    // ... send the order ...
    if (!log_ring.try_push({id, px})) { /* ring full: count a dropped log line, never block */ }
}

int main() {
    std::thread cold([] {                                   // formatting and I/O live here
        SentEvent e;
        for (;;) {
            bool finished = done.load(std::memory_order_acquire);   // read the flag FIRST
            while (log_ring.try_pop(e))
                std::printf("sent id=%llu px=%lld\n",
                            (unsigned long long)e.order_id, (long long)e.price_ticks);
            if (finished) break;   // all pushes made before `done` was set are drained
        }
    });
    for (std::uint64_t id = 1; id <= 3; ++id) hot_path_send(id, 4105000 + id);
    done.store(true, std::memory_order_release);
    cold.join();
}
