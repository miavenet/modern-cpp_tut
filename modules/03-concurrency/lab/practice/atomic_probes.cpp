// atomic_probes.cpp - four ways to ask "is the shared word equal to k?" (same orders as atomic_probes_builtin.c).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_probes.cpp -o -
//   then the same with -march=armv8-a, and with -mcpu=apple-a7; add -### to see the CPU the driver passes.
#include <atomic>
#include <cstdint>

bool probe_load_acq(const std::atomic<std::uint64_t>& a, std::uint64_t k) {
  return a.load(std::memory_order_acquire) == k;
}
bool probe_load_seq(const std::atomic<std::uint64_t>& a, std::uint64_t k) {
  return a.load(std::memory_order_seq_cst) == k;
}
// "Equality test" spelled as a compare-exchange that writes back k on success.
bool probe_cas_relaxed(std::atomic<std::uint64_t>& a, std::uint64_t k) {
  std::uint64_t expected = k;
  return a.compare_exchange_strong(expected, k, std::memory_order_relaxed);
}
bool probe_cas_seq(std::atomic<std::uint64_t>& a, std::uint64_t k) {
  std::uint64_t expected = k;
  return a.compare_exchange_strong(expected, k, std::memory_order_seq_cst);
}
