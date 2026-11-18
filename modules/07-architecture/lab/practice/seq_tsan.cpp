// seq_tsan.cpp - one writer, one reader, version-counter protocol; COPY=0 plain copy, COPY=1 atomic_ref words. Run under ThreadSanitizer.
// Build and run from the tutorial root; the program goes to your temp directory:
//   for c in 0 1; do clang++ -std=c++20 -O1 -g -fsanitize=thread -Wall -Wextra -DCOPY=$c modules/07-architecture/lab/practice/seq_tsan.cpp -o "${TMPDIR:-/tmp}/seq_tsan$c" && for r in 1 2 3; do "${TMPDIR:-/tmp}/seq_tsan$c"; done; done
// One writer, one reader, version-counter protocol. COPY=0: plain struct copy.
// COPY=1: each word copied with relaxed atomic_ref loads/stores. Run under ThreadSanitizer.
#include <atomic>
#include <cstdio>
#include <thread>
#ifndef COPY
#define COPY 0
#endif
struct Pos { long qty[4]; };
Pos g_pos;
std::atomic<unsigned> g_seq{0};
std::atomic<int> g_started{0};
long g_retries = 0;
void write(long v) {
    unsigned s = g_seq.load(std::memory_order_relaxed);
    g_seq.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    for (int i = 0; i < 4; ++i) {
#if COPY
        std::atomic_ref<long>(g_pos.qty[i]).store(v, std::memory_order_relaxed);
#else
        g_pos.qty[i] = v;
#endif
    }
    g_seq.store(s + 2, std::memory_order_release);
}
Pos read() {
    Pos out;
    unsigned a, b;
    do {
        a = g_seq.load(std::memory_order_acquire);
        for (int i = 0; i < 4; ++i) {
#if COPY
            out.qty[i] = std::atomic_ref<long>(g_pos.qty[i]).load(std::memory_order_relaxed);
#else
            out.qty[i] = g_pos.qty[i];
#endif
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        b = g_seq.load(std::memory_order_relaxed);
        if ((a & 1) || a != b) ++g_retries; else break;
    } while (true);
    return out;
}
int main() {
    std::thread w([] { g_started.fetch_add(1); while (g_started.load() < 2) {} for (long v = 1; v <= 200000; ++v) write(v); });
    long bad = 0;
    std::thread r([&] {
        g_started.fetch_add(1); while (g_started.load() < 2) {}
        for (int k = 0; k < 200000; ++k) {
            Pos p = read();
            for (int i = 1; i < 4; ++i) bad += (p.qty[i] != p.qty[0]);
        }
    });
    w.join(); r.join();
    std::printf("COPY=%d torn=%ld retries=%ld\n", COPY, bad, g_retries);
}
