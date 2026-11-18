// handoff_tail.cpp - LPA lens (lesson 05): thread hand-off by blocking vs by spinning.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/03-concurrency/lab/handoff_tail.cpp -o "${TMPDIR:-/tmp}/handoff_tail" && "${TMPDIR:-/tmp}/handoff_tail"
// Two threads play ping-pong 200,000 times, one round every 20 us (like orders
// arriving with gaps; the gap itself is not timed). Each sample is one full round
// trip (ping -> responder wakes -> pong -> pinger wakes). Four ways to wait:
//   condvar        : std::mutex + std::condition_variable both ways (blocks in the kernel)
//   atomic wait    : C++20 std::atomic::wait / notify_one
//   spin           : busy-poll an atomic with acquire loads (never sleeps)
//   spin, E-core   : spin, but the responder asked for background QoS, a low-priority
//                    class; observed effect: a much slower far tail (the OS reports
//                    no core number, so we see the effect, not where it ran)
// Counter: CPU time / wall time = how many cores the hand-off kept busy, gaps included
// (the pinger busy-waits through its gaps, so 1.0 is the pinger alone).
#include "lat_hist.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <pthread.h>
#include <sys/resource.h>
#include <thread>
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

constexpr int kRounds = 200'000;
constexpr std::uint64_t kGapNs = 20'000;   // time between rounds, not timed

static double cpu_seconds() {
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    auto sec = [](timeval t) { return static_cast<double>(t.tv_sec) + t.tv_usec * 1e-6; };
    return sec(r.ru_utime) + sec(r.ru_stime);
}

static void demote_to_background() {
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#endif
}

struct cv_channel {
    std::mutex m;
    std::condition_variable cv;
    int turn = 0;                    // 0: pinger's move, 1: responder's move
};

template <class Ping, class Responder>
static void play(const char* label, Ping ping, Responder respond, std::uint64_t floor_ns) {
    static lat::histogram h;
    h.reset();
    std::thread t(respond);
    const double c0 = cpu_seconds();
    const std::uint64_t w0 = lat::now_ns();
    std::uint64_t next = w0;
    for (int i = 0; i < kRounds; ++i) {
        next += kGapNs;
        while (lat::now_ns() < next) {}          // the gap: not part of the sample
        const std::uint64_t t0 = lat::now_ns();
        ping();
        h.record(lat::now_ns() - t0);
    }
    const double wall = static_cast<double>(lat::now_ns() - w0) * 1e-9;
    const double cpu = cpu_seconds() - c0;
    t.join();
    h.report(stdout, label, floor_ns);
    std::printf("               CPU time / wall time = %.2f cores\n", cpu / wall);
    std::fflush(stdout);
}

int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("ping-pong x %d round trips per row, one every %llu ns; environment: clang %s, %s, %u hardware threads, "
                "no core pinning (macOS), shared machine\n",
                kRounds, static_cast<unsigned long long>(kGapNs), __clang_version__, opt, std::thread::hardware_concurrency());

    {   // 1. block on a condition variable, both directions
        cv_channel c;
        play("condvar",
             [&] {
                 std::unique_lock lk(c.m);
                 c.turn = 1;
                 c.cv.notify_all();
                 c.cv.wait(lk, [&] { return c.turn == 0; });
             },
             [&] {
                 for (int i = 0; i < kRounds; ++i) {
                     std::unique_lock lk(c.m);
                     c.cv.wait(lk, [&] { return c.turn == 1; });
                     c.turn = 0;
                     c.cv.notify_all();
                 }
             }, floor_ns);
    }
    {   // 2. C++20 atomic wait / notify
        std::atomic<int> turn{0};
        play("atomic wait",
             [&] {
                 turn.store(1, std::memory_order_release);
                 turn.notify_one();
                 turn.wait(1, std::memory_order_acquire);   // sleeps while turn == 1
             },
             [&] {
                 for (int i = 0; i < kRounds; ++i) {
                     turn.wait(0, std::memory_order_acquire);
                     turn.store(0, std::memory_order_release);
                     turn.notify_one();
                 }
             }, floor_ns);
    }
    for (int ecore = 0; ecore < 2; ++ecore) {   // 3 and 4. busy-poll
        alignas(128) std::atomic<int> turn{0};
        play(ecore ? "spin, E-core" : "spin",
             [&] {
                 turn.store(1, std::memory_order_release);
                 while (turn.load(std::memory_order_acquire) != 0) {}
             },
             [&, ecore] {
                 if (ecore) demote_to_background();
                 for (int i = 0; i < kRounds; ++i) {
                     while (turn.load(std::memory_order_acquire) != 1) {}
                     turn.store(0, std::memory_order_release);
                 }
             }, floor_ns);
    }
}
