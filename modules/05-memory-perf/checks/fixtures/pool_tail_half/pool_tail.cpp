// Module 05 LPA lab, CHECK FIXTURE (deliberately wrong prepare_for_peak).
//
// Build and run from the tutorial root (needs your finished pool_lab.cpp next to it):
//   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/05-memory-perf/lab/solution/pool_tail.cpp \
//       -o "${TMPDIR:-/tmp}/pool_tail" && "${TMPDIR:-/tmp}/pool_tail"
//
// The same burst (2,000,000 orders pushed into an empty book, timed in batches
// of 16) runs three ways, each in a fresh child process so one run's freed
// memory cannot warm the next:
//   default  : std::pmr::list over the general heap (via a counting wrapper)
//   lazy     : the lab's fixed_pool, fetching chunks when it runs dry
//   prepared : the same pool, prepared for the peak BEFORE the clock starts
// It prints a latency histogram per run plus two counters: heap calls made
// while the clock ran (graded) and minor page faults (shown, not graded).

#define main pool_example_main
#include "pool_lab.cpp"
#undef main

#include "lat_hist.hpp"

#include <cstdlib>
#include <cstring>
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
void* operator new(std::size_t n, std::align_val_t a) {
    ++g_global_news;
    std::size_t al = static_cast<std::size_t>(a);
    if (al < sizeof(void*)) al = sizeof(void*);
    void* p = nullptr;
    if (posix_memalign(&p, al, n ? n : 1) == 0) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }

constexpr std::size_t kPeak = 2'000'000;   // orders alive at the end of the burst
constexpr int kBatch = 16;                 // push_backs per timed sample
constexpr std::size_t kBlocksPerChunk = 4096;

static long minor_faults() {
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return r.ru_minflt;
}

// TODO(M5-LPA2) in the starter: pay the pool's one-off costs before the clock
// starts. After this returns, the book is empty and the pool already holds
// enough blocks, fetched and touched, for `peak` live orders.
void prepare_for_peak(std::pmr::list<Order>& book, std::size_t peak) {
    for (std::size_t i = 0; i < peak / 2; ++i)
        book.push_back(Order{i, 100.0, 1, 'B'});
    book.clear();   // every node returns to the pool's free list
}

struct run_result { std::size_t heap_calls; long faults; };

static run_result timed_burst(std::pmr::list<Order>& book, lat::histogram& h) {
    h.reset();                                   // touch the histogram before timing
    const std::size_t news0 = g_global_news;
    const long faults0 = minor_faults();
    for (std::size_t i = 0; i < kPeak; i += kBatch) {
        const std::uint64_t t0 = lat::now_ns();
        for (int j = 0; j < kBatch; ++j)
            book.push_back(Order{i + j, 100.0, 1, 'B'});
        h.record(lat::now_ns() - t0);
    }
    const long faults = minor_faults() - faults0;
    return {g_global_news - news0, faults};   // every heap path ends in operator new
}

static int child(const char* mode, std::uint64_t floor_ns) {
    static lat::histogram h;                     // static: 15 KB, no stack worry
    counting_resource heap;                      // the lab's upstream: plain heap underneath
    run_result r{};
    if (std::strcmp(mode, "default") == 0) {
        std::pmr::list<Order> book(&heap);
        r = timed_burst(book, h);
        h.report(stdout, "default (heap)", floor_ns);
    } else {
        fixed_pool pool(kBlocksPerChunk, &heap);
        std::pmr::list<Order> book(&pool);
        const bool prepared = std::strcmp(mode, "prepared") == 0;
        if (prepared) prepare_for_peak(book, kPeak);
        r = timed_burst(book, h);
        h.report(stdout, prepared ? "pool, prepared" : "pool, lazy", floor_ns);
    }
    std::printf("               heap calls while timed = %zu, minor page faults = %ld\n",
                r.heap_calls, r.faults);
    std::fflush(stdout);
    return r.heap_calls == 0 ? 0 : 3;            // exit 0 means "no heap calls"
}

#ifndef POOL_TAIL_NO_MAIN   // the course check defines this to call prepare_for_peak directly
int main() {
    const std::uint64_t floor_ns = lat::clock_floor_ns();
#if defined(__OPTIMIZE__)
    const char* opt = "optimised";
#else
    const char* opt = "NOT optimised (add -O2)";
#endif
    std::printf("burst: %zu orders into an empty std::pmr::list<Order>, timed per %d "
                "push_backs (%zu samples per run)\n",
                kPeak, kBatch, kPeak / kBatch);
    std::printf("environment: clang %s, %s, one thread, no core pinning (macOS), "
                "shared machine; numbers vary run to run\n", __clang_version__, opt);
    std::fflush(stdout);
    const char* modes[] = {"default", "lazy", "prepared"};
    int status[3] = {};
    for (int i = 0; i < 3; ++i) {
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); return 2; }
        if (pid == 0) std::_Exit(child(modes[i], floor_ns));
        int st = 0;
        waitpid(pid, &st, 0);
        status[i] = WIFEXITED(st) ? WEXITSTATUS(st) : 99;
    }
    const bool default_used_heap = status[0] == 3;
    const bool lazy_used_heap = status[1] == 3;
    const bool prepared_clean = status[2] == 0;
    if (default_used_heap && lazy_used_heap && prepared_clean) {
        std::puts("RESULT: PASS - the prepared pool made no heap calls while the clock ran");
        return 0;
    }
    if (!prepared_clean)
        std::puts("RESULT: TODO - the prepared run still calls the heap while timed; "
                  "finish prepare_for_peak");
    else
        std::puts("RESULT: FAIL - a baseline run made no heap calls; did you change "
                  "the default or lazy runs?");
    return 1;
}
#endif
