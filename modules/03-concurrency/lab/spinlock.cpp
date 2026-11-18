// spinlock.cpp - a test-and-test-and-set spinlock with a CPU spin hint.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -g -fsanitize=thread modules/03-concurrency/lab/spinlock.cpp -o "${TMPDIR:-/tmp}/spinlock" && "${TMPDIR:-/tmp}/spinlock"
#include <atomic>
#include <cstdio>
#include <thread>
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
inline void cpu_relax() { _mm_pause(); }               // x86: PAUSE
#elif defined(__aarch64__)
inline void cpu_relax() { __builtin_arm_yield(); }     // arm64 (clang/gcc): YIELD
#else
inline void cpu_relax() {}
#endif

class Spinlock {
    std::atomic_flag flag_;                             // C++20: starts clear
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) // try to take it
            while (flag_.test(std::memory_order_relaxed))      // C++20: read-only wait
                cpu_relax();
    }
    void unlock() { flag_.clear(std::memory_order_release); }
};

Spinlock lock;
long total = 0;                                         // protected by lock

int main() {
    auto work = [] { for (int i = 0; i < 100000; ++i) { lock.lock(); ++total; lock.unlock(); } };
    std::thread a(work), b(work);
    a.join(); b.join();
    std::printf("total = %ld\n", total);
}
