// first_touch.cpp -- count the page faults caused by FIRST touch of memory.
// Run from the tutorial root:
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/hotpath/first_touch.cpp -o "${TMPDIR:-/tmp}/first_touch" && "${TMPDIR:-/tmp}/first_touch"
#include <sys/resource.h>
#include <cstdio>
#include <cstring>
#include <memory>

static long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;
}

int main() {
    constexpr std::size_t kBytes = 64 * 1024 * 1024;           // a 64 MiB "pool"
    std::unique_ptr<char[]> pool(new char[kBytes]);            // allocated, not touched
    long before = minor_faults();
    std::memset(pool.get(), 1, kBytes);                        // first touch
    asm volatile("" : : "r"(pool.get()) : "memory");           // keep both writes (GCC/clang)
    long after_first = minor_faults();
    std::memset(pool.get(), 2, kBytes);                        // second touch
    asm volatile("" : : "r"(pool.get()) : "memory");           // keep both writes (GCC/clang)
    long after_second = minor_faults();
    std::printf("faults during first touch:  %ld\n", after_first - before);
    std::printf("faults during second touch: %ld\n", after_second - after_first);
    return pool[12345] == 2 ? 0 : 1;
}
