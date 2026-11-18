// Module 01 LPA lab, REFERENCE SOLUTION. Try lab/starter/push_tail.cpp first.
//
// Build and run from the tutorial root:
//   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/01-foundations/lab/solution/push_tail.cpp \
//       -o "${TMPDIR:-/tmp}/push_tail" && "${TMPDIR:-/tmp}/push_tail"
//
// The same burst (2,000,000 orders moved into an empty std::vector, timed in
// batches of 16 push_backs) runs four ways, each in a fresh child process:
//   copy-growth : LegacyOrder, whose move constructor is NOT noexcept, so every
//                 growth step COPIES all old elements (and their strings)
//   move-growth : Order (your noexcept move), growing as it goes
//   reserve only: Order, capacity reserved before the clock, pages untouched
//   prepared    : Order, prepare_for_peak() before the clock (your code)
// It prints a latency histogram per run and three counters taken while the
// clock ran: element copies, heap calls, minor page faults.

#include "lat_hist.hpp"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

// Count every global operator new, so a hidden heap call cannot hide.
static std::size_t g_global_news = 0;
void* operator new(std::size_t n) {
    ++g_global_news;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static std::size_t g_copies = 0;   // element copy constructions

// The C++98-style order: it has a move constructor, but did not promise noexcept.
struct LegacyOrder {
    std::uint64_t id = 0;
    double px = 0;
    std::string tag;                                     // 40 chars: lives on the heap
    LegacyOrder() = default;
    LegacyOrder(std::uint64_t i, double p, std::string t) : id(i), px(p), tag(std::move(t)) {}
    LegacyOrder(const LegacyOrder& o) : id(o.id), px(o.px), tag(o.tag) { ++g_copies; }
    LegacyOrder(LegacyOrder&& o) : id(o.id), px(o.px), tag(std::move(o.tag)) {}
    LegacyOrder& operator=(const LegacyOrder&) = default;
    LegacyOrder& operator=(LegacyOrder&&) = default;
};

// TODO(M1-LPA2) part 1 in the starter: let std::vector MOVE Order when it grows.
struct Order {
    std::uint64_t id = 0;
    double px = 0;
    std::string tag;
    Order() = default;
    Order(std::uint64_t i, double p, std::string t) : id(i), px(p), tag(std::move(t)) {}
    Order(const Order& o) : id(o.id), px(o.px), tag(o.tag) { ++g_copies; }
    Order(Order&& o) noexcept : id(o.id), px(o.px), tag(std::move(o.tag)) {}
    Order& operator=(const Order&) = default;
    Order& operator=(Order&&) noexcept = default;
};

constexpr std::size_t kPeak = 2'000'000;   // orders in the book at the end of the burst
constexpr int kBatch = 16;                 // push_backs per timed sample

// TODO(M1-LPA2) part 2 in the starter: pay the vector's one-off costs before the
// clock starts. Afterwards the book is empty and can take `peak` orders with no
// heap call and no page fault.
void prepare_for_peak(std::vector<Order>& book, std::size_t peak) {
    book.reserve(peak);   // one allocation, now
    book.resize(peak);    // first-touch every page (empty tags: no heap calls)
    book.clear();         // size 0 again; capacity and touched pages stay
}

static long minor_faults() {
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return r.ru_minflt;
}

template <class T>
static std::vector<T> make_source() {
    std::vector<T> src;
    src.reserve(kPeak);
    for (std::size_t i = 0; i < kPeak; ++i)
        src.emplace_back(i, 100.0, std::string(40, static_cast<char>('a' + i % 26)));
    return src;
}

struct counters { std::size_t copies, heap_calls; long faults; };

template <class T>
static counters timed_burst(std::vector<T>& book, std::vector<T>& src, lat::histogram& h) {
    h.reset();
    const std::size_t c0 = g_copies, n0 = g_global_news;
    const long f0 = minor_faults();
    for (std::size_t i = 0; i < kPeak; i += kBatch) {
        const std::uint64_t t0 = lat::now_ns();
        for (int j = 0; j < kBatch; ++j) book.push_back(std::move(src[i + j]));
        h.record(lat::now_ns() - t0);
    }
    const long faults = minor_faults() - f0;
    return {g_copies - c0, g_global_news - n0, faults};
}

static int child(int mode, std::uint64_t floor_ns) {
    static lat::histogram h;
    counters c{};
    const char* label = "";
    if (mode == 0) {
        auto src = make_source<LegacyOrder>();
        std::vector<LegacyOrder> book;
        c = timed_burst(book, src, h);
        label = "copy-growth";
    } else {
        auto src = make_source<Order>();
        std::vector<Order> book;
        if (mode == 2) book.reserve(kPeak);
        if (mode == 3) prepare_for_peak(book, kPeak);
        c = timed_burst(book, src, h);
        label = mode == 1 ? "move-growth" : mode == 2 ? "reserve only" : "prepared";
    }
    h.report(stdout, label, floor_ns);
    std::printf("               copies while timed = %zu, heap calls while timed = %zu, "
                "minor page faults = %ld\n", c.copies, c.heap_calls, c.faults);
    std::fflush(stdout);
    // exit code: bit 0 = made copies, bit 1 = made heap calls
    return (c.copies ? 1 : 0) | (c.heap_calls ? 2 : 0);
}

#ifndef PUSH_TAIL_NO_MAIN   // the course check defines this to call your code directly
int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("burst: %zu orders moved into an empty std::vector, timed per %d "
                "push_backs (%zu samples per run)\n", kPeak, kBatch, kPeak / kBatch);
    std::printf("environment: clang %s, %s, one thread, no core pinning (macOS), "
                "shared machine; numbers vary run to run\n", __clang_version__, opt);
    std::fflush(stdout);
    int status[4] = {};
    for (int m = 0; m < 4; ++m) {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); return 2; }
        if (pid == 0) std::_Exit(child(m, floor_ns));
        int st = 0;
        waitpid(pid, &st, 0);
        status[m] = WIFEXITED(st) ? WEXITSTATUS(st) : 99;
    }
    const bool baseline_copied = status[0] & 1;
    const bool growth_moves = status[1] == 2;      // no copies, but it did call the heap
    const bool prepared_clean = status[3] == 0;
    if (baseline_copied && growth_moves && prepared_clean) {
        std::puts("RESULT: PASS - growth moves instead of copying, and the prepared "
                  "book made no copies and no heap calls while the clock ran");
        return 0;
    }
    if (!growth_moves)
        std::puts("RESULT: TODO - move-growth still copies: make Order's move noexcept");
    if (!prepared_clean)
        std::puts("RESULT: TODO - the prepared book still calls the heap while timed: "
                  "finish prepare_for_peak");
    if (growth_moves && prepared_clean && !baseline_copied)
        std::puts("RESULT: FAIL - the copy-growth baseline made no copies; "
                  "did you change LegacyOrder?");
    return 1;
}
#endif
