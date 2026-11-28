// wide_cas.cpp - is a 16-byte {pointer, tag} atomic lock-free, and what does its compare-and-swap become?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
//   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -   (add --target=x86_64-apple-macos13, then also -march=x86-64, then also -mcx16)
//   On Linux/libstdc++ both GCC and Clang need -latomic (not on the Mac): clang++ (or g++) -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -latomic -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
#include <atomic>
#include <cstdint>
#include <cstdio>
struct alignas(16) Tagged { std::uint64_t ptr; std::uint64_t tag; };
std::atomic<Tagged> slot{};
bool try_swap(Tagged expected, Tagged desired) {
  return slot.compare_exchange_strong(expected, desired);
}
#ifdef MAIN
int main() {
  std::printf("is_always_lock_free(16B)=%d is_lock_free()=%d\n",
              (int)std::atomic<Tagged>::is_always_lock_free, (int)slot.is_lock_free());
  std::printf("is_always_lock_free(8B)=%d\n", (int)std::atomic<std::uint64_t>::is_always_lock_free);
}
#endif
