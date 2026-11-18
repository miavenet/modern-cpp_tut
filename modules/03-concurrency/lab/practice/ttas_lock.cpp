// ttas_lock.cpp - a test-and-test-and-set lock guarding a plain counter; -DRELAXED makes every order relaxed.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -DOK modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
//   clang++ -std=c++20 -O2 -Wall -Wextra -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
//   clang++ -std=c++20 -O1 -g -fsanitize=thread -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
// A test-and-test-and-set lock guarding a plain counter. ORD_ACQ / ORD_REL pick the orders.
// Gate: final counter must equal THREADS * ITERS.
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>
#ifdef RELAXED
#define ORD_ACQ std::memory_order_relaxed
#define ORD_REL std::memory_order_relaxed
#else
#define ORD_ACQ std::memory_order_acquire
#define ORD_REL std::memory_order_release
#endif
struct TTAS {
  std::atomic<bool> busy{false};
  void lock() {
    for (;;) {
      if (!busy.exchange(true, ORD_ACQ)) return;
      while (busy.load(std::memory_order_relaxed)) { }   // spin on a read
    }
  }
  void unlock() { busy.store(false, ORD_REL); }
};
int main() {
  constexpr int THREADS = 4, ITERS = 20000;
  TTAS m; long counter = 0;
  std::vector<std::thread> ts;
  for (int t = 0; t < THREADS; ++t)
    ts.emplace_back([&] { for (int i = 0; i < ITERS; ++i) { m.lock(); ++counter; m.unlock(); } });
  for (auto& th : ts) th.join();
  std::printf("counter=%ld expected=%ld %s\n", counter, (long)THREADS * ITERS,
              counter == (long)THREADS * ITERS ? "PASS" : "FAIL");
}
