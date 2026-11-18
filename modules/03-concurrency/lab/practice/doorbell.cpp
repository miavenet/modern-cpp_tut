// doorbell.cpp - a producer rings every `gap` microseconds; a consumer waits for each ring.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/doorbell.cpp -o "${TMPDIR:-/tmp}/doorbell" && "${TMPDIR:-/tmp}/doorbell" atomic 20 66666 && "${TMPDIR:-/tmp}/doorbell" cv 20 66666
// Usage: doorbell <atomic|cv> <gap_us> <rings>
// Measures the consumer's CPU time per ring (thread CPU clock) and its wake-up delay.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>
#include <vector>

using clk = std::chrono::steady_clock;
static int64_t ns(clk::time_point t) { return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count(); }
static int64_t thread_cpu_ns() { timespec ts; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return ts.tv_sec * 1000000000LL + ts.tv_nsec; }

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: db <atomic|cv> <gap_us> <rings>\n"); return 2; }
  const bool use_atomic = !strcmp(argv[1], "atomic");
  const int64_t gap = atoll(argv[2]) * 1000;
  const int R = atoi(argv[3]);
  std::vector<int64_t> sent(R + 1, 0), delay;
  delay.reserve(R);

  std::atomic<int> seq{0};      // atomic mode
  std::mutex mu; std::condition_variable cv; int cseq = 0;  // cv mode
  int64_t cpu_used = 0; int wakes = 0;

  std::thread consumer([&] {
    int last = 0;
    int64_t c0 = thread_cpu_ns();
    while (last < R) {
      int now_seq;
      if (use_atomic) {
        seq.wait(last, std::memory_order_acquire);
        now_seq = seq.load(std::memory_order_acquire);
      } else {
        std::unique_lock lk(mu);
        cv.wait(lk, [&] { return cseq != last; });
        now_seq = cseq;
      }
      int64_t t = ns(clk::now());
      delay.push_back(t - sent[now_seq]);
      last = now_seq; ++wakes;
    }
    cpu_used = thread_cpu_ns() - c0;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));  // let the consumer start waiting
  int64_t next = ns(clk::now());
  for (int i = 1; i <= R; ++i) {
    next += gap;
    while (ns(clk::now()) < next) {}            // producer busy-waits for the gap (it is not measured)
    sent[i] = ns(clk::now());
    if (use_atomic) { seq.store(i, std::memory_order_release); seq.notify_one(); }
    else { { std::lock_guard lk(mu); cseq = i; } cv.notify_one(); }
  }
  consumer.join();
  std::sort(delay.begin(), delay.end());
  auto pct = [&](double p) { return (long long)delay[(size_t)(p * (delay.size() - 1))]; };
  printf("mode=%-6s gap=%4lld us rings=%d wakes=%d | consumer CPU per ring %.2f us (%.0f%% of the gap) | wake delay p50=%lld p99=%lld ns\n",
         argv[1], (long long)(gap / 1000), R, wakes, cpu_used / 1000.0 / R,
         gap ? 100.0 * cpu_used / ((double)gap * R) : 0.0, pct(0.5), pct(0.99));
}
