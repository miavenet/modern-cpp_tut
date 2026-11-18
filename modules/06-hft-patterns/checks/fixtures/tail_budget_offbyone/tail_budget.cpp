// CHECK FIXTURE (deliberately wrong: tail_budget_offbyone). Module 06 lesson 05 capstone. Try lab/budget/starter/tail_budget.cpp first.
//
// Build and run from the tutorial root:
//   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure -I modules/06-hft-patterns/lab/spsc/solution modules/06-hft-patterns/lab/budget/solution/tail_budget.cpp \
//       -o "${TMPDIR:-/tmp}/tail_budget" && "${TMPDIR:-/tmp}/tail_budget"
//
// A feed thread hands 1,000,000 messages, one every 2 us, through the course's SPSC
// ring to a strategy thread. Every 100,000th message the strategy takes a position
// snapshot that costs about 300 us. Three runs, each in a fresh child process:
//   closed loop : the feed sends the next message only after the strategy finished
//                 the previous one (how many test clients work: the mistake)
//   open loop   : the feed follows its own schedule, intended_send_ns() (your code;
//                 this run and the next are skipped until that schedule is open loop)
//   open, sliced: same schedule; the snapshot is cut into 1 us slices, one per message
// Per run it prints the end-to-end histogram, measured from each message's INTENDED
// send time, and one histogram per stage:
//   send lateness = sent - intended        (feed thread)
//   queue wait    = dequeued - sent         (ring + strategy busy elsewhere)
//   service       = done - dequeued         (strategy's own work)
// Budget for this exercise: end-to-end p99.9 <= 20 us and p99.99 <= 50 us.

#include "lat_hist.hpp"
#include "spsc_ring.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

constexpr std::uint64_t kMessages = 1'000'000;
constexpr std::uint64_t kIntervalNs = 2'000;        // one message every 2 us
constexpr std::uint64_t kSnapshotEvery = 100'000;
constexpr std::uint64_t kSnapshotNs = 300'000;      // 300 us of position snapshot
constexpr std::uint64_t kSliceNs = 1'000;           // sliced: 1 us per message
constexpr std::uint64_t kBudgetP999 = 20'000, kBudgetP9999 = 50'000;

// TODO(M6-LPA5) in the starter: when should message i be sent?
//   start_ns     : when the feed began
//   prev_done_ns : when the strategy finished message i-1 (0 for i == 0)
// An open-loop feed keeps its own clock: a stall downstream must not delay the
// schedule, or the stalled period is silently left out of the measurement.
std::uint64_t intended_send_ns(std::uint64_t start_ns, std::uint64_t i,
                               std::uint64_t prev_done_ns) {
    (void)prev_done_ns;
    return start_ns + (i + 1) * kIntervalNs;   // wrong: first message one interval late
}

struct Msg { std::uint64_t seq, intended_ns, sent_ns; };

static void spin_for(std::uint64_t ns) {
    const std::uint64_t end = lat::now_ns() + ns;
    while (lat::now_ns() < end) {}
}

enum class mode { closed, open, sliced };

struct stage_hists { lat::histogram e2e, late, queue, service; };

static void run(mode m, const char* label, std::uint64_t floor_ns) {
    static SpscRing<Msg, 4096> ring;                 // static: 128 KB, reused across runs
    static stage_hists h;
    h.e2e.reset(); h.late.reset(); h.queue.reset(); h.service.reset();
    std::atomic<std::uint64_t> done_count{0}, last_done_ns{0};
    std::uint64_t over_budget = 0;

    std::thread strategy([&] {
        std::uint64_t debt = 0;                      // sliced snapshot work still owed
        double position = 0;
        for (std::uint64_t got = 0; got < kMessages;) {
            Msg msg;
            if (!ring.try_pop(msg)) continue;        // busy-poll: never sleeps
            const std::uint64_t deq = lat::now_ns();
            position += 1e-6 * static_cast<double>(msg.seq & 7);   // the normal work
            if (msg.seq % kSnapshotEvery == kSnapshotEvery - 1) {
                if (m == mode::sliced) debt += kSnapshotNs; else spin_for(kSnapshotNs);
            }
            if (debt) { spin_for(kSliceNs); debt -= kSliceNs; }
            const std::uint64_t done = lat::now_ns();
            h.late.record(msg.sent_ns - msg.intended_ns);
            h.queue.record(deq - msg.sent_ns);
            h.service.record(done - deq);
            h.e2e.record(done - msg.intended_ns);
            if (done - msg.intended_ns > kBudgetP9999) ++over_budget;
            last_done_ns.store(done, std::memory_order_relaxed);
            done_count.store(++got, std::memory_order_release);
        }
        if (position < 0) std::puts("");
    });

    const std::uint64_t start = lat::now_ns() + 1'000'000;   // 1 ms to let the strategy spin up
    for (std::uint64_t i = 0; i < kMessages; ++i) {
        std::uint64_t intended;
        if (m == mode::closed) {
            while (done_count.load(std::memory_order_acquire) < i) {}   // wait for the reply
            const std::uint64_t prev = i ? last_done_ns.load(std::memory_order_relaxed) : start;
            intended = prev + kIntervalNs;           // then pace from the reply
        } else {
            intended = intended_send_ns(start, i, i ? last_done_ns.load(std::memory_order_relaxed) : 0);
        }
        while (lat::now_ns() < intended) {}
        Msg msg{i, intended, 0};
        msg.sent_ns = lat::now_ns();
        while (!ring.try_push(msg)) {}               // full ring: the feed waits (shows as lateness)
    }
    strategy.join();

    std::printf("== %s\n", label);
    h.e2e.report(stdout, "end-to-end", floor_ns);
    h.late.report(stdout, "send lateness", floor_ns);
    h.queue.report(stdout, "queue wait", floor_ns);
    h.service.report(stdout, "service", floor_ns);
    const bool ok = h.e2e.value_or_zero(0.999) <= kBudgetP999 &&
                    h.e2e.value_or_zero(0.9999) <= kBudgetP9999;
    std::printf("               messages over 50 us = %llu; budget (p99.9 <= 20 us, p99.99 <= 50 us): %s\n",
                static_cast<unsigned long long>(over_budget), ok ? "MET" : "BROKEN");
    std::fflush(stdout);
}

// The schedule is open-loop when it does not depend on when the previous reply came.
static bool schedule_is_open_loop() {
    const std::uint64_t s = 5'000;
    for (std::uint64_t i : {1ull, 2ull, 777ull, 999'999ull})
        for (std::uint64_t prev : {0ull, s + i * kIntervalNs + 123'456ull, 1ull << 40})
            if (intended_send_ns(s, i, prev) != s + i * kIntervalNs) return false;
    return true;
}

#ifndef TAIL_BUDGET_NO_MAIN   // the course check defines this to call your code directly
int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("%llu messages, one every %llu ns, snapshot %llu ns every %llu messages\n",
                static_cast<unsigned long long>(kMessages), static_cast<unsigned long long>(kIntervalNs),
                static_cast<unsigned long long>(kSnapshotNs), static_cast<unsigned long long>(kSnapshotEvery));
    std::printf("environment: clang %s, %s, two busy threads, no core pinning (macOS), shared machine\n",
                __clang_version__, opt);
    std::fflush(stdout);
    // Each run gets a fresh child process. When the runs shared one process, whichever
    // open-loop run came SECOND had a much worse tail (course Mac, ingest evidence); the
    // likely cause is the scheduler deprioritising threads that have been busy for seconds.
    auto in_child = [&](mode m, const char* label) {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); std::exit(2); }
        if (pid == 0) { run(m, label, floor_ns); std::_Exit(0); }
        int st = 0;
        waitpid(pid, &st, 0);
    };
    in_child(mode::closed, "closed loop (feed waits for each reply)");
    if (!schedule_is_open_loop()) {
        std::puts("== open-loop runs skipped: intended_send_ns still depends on the previous reply");
        std::puts("RESULT: TODO - finish intended_send_ns (open loop), then run again");
        return 1;
    }
    in_child(mode::open, "open loop (your intended_send_ns)");
    in_child(mode::sliced, "open loop, snapshot sliced");
    std::puts("RESULT: PASS - the feed keeps its own schedule (open loop)");
    return 0;
}
#endif
