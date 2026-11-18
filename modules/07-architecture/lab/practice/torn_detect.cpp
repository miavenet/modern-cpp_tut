// torn_detect.cpp - a self-checking payload: any snapshot whose eight words differ is torn (argument none or seq).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/torn_detect.cpp -o "${TMPDIR:-/tmp}/torn_detect" && for r in 1 2 3; do "${TMPDIR:-/tmp}/torn_detect" none && "${TMPDIR:-/tmp}/torn_detect" seq; done
// Self-checking payload: the writer always stores the SAME value into all 8 words.
// Any snapshot whose words differ is torn. Words are relaxed atomics, so there is no
// data race (no UB) even in the unprotected mode; tearing is still possible.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
struct Snap { long w[8]; };
std::atomic<long> g_w[8];
std::atomic<unsigned> g_seq{0};
std::atomic<bool> g_stop{false}, g_go{false};
int main(int argc, char** argv) {
    bool protect = argc > 1 && !std::strcmp(argv[1], "seq");
    std::thread writer([&] {
        while (!g_go.load()) {}
        for (long v = 1; !g_stop.load(std::memory_order_relaxed); ++v) {
            unsigned s = g_seq.load(std::memory_order_relaxed);
            if (protect) { g_seq.store(s + 1, std::memory_order_relaxed);
                           std::atomic_thread_fence(std::memory_order_release); }
            for (auto& x : g_w) x.store(v, std::memory_order_relaxed);
            if (protect) g_seq.store(s + 2, std::memory_order_release);
        }
    });
    long reads = 0, torn = 0;
    g_go.store(true);
    for (int k = 0; k < 2'000'000; ++k) {
        Snap s; unsigned a, b;
        do {
            a = g_seq.load(std::memory_order_acquire);
            for (int i = 0; i < 8; ++i) s.w[i] = g_w[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            b = g_seq.load(std::memory_order_relaxed);
        } while (protect && ((a & 1) || a != b));
        ++reads;
        for (int i = 1; i < 8; ++i) if (s.w[i] != s.w[0]) { ++torn; break; }
    }
    g_stop.store(true); writer.join();
    std::printf("mode=%s reads=%ld torn=%ld\n", protect ? "seq" : "none", reads, torn);
}
