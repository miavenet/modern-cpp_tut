// noise.cpp - a "background load" function meant to keep the memory system busy, in two versions.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/07-architecture/lab/practice/noise.cpp -o -   (read the two function bodies; _Znwm is operator new)
// A "background load" function meant to keep the memory system busy, in two versions.
#include <cstddef>
#include <cstdint>
#include <vector>
// Version A: fills a scratch buffer that nothing reads afterwards.
__attribute__((noinline)) void noise_plain(std::size_t n) {
  std::vector<std::uint32_t> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::uint32_t>(i) * 2654435761u;
}
// Version B: the same fill through a volatile pointer.
__attribute__((noinline)) void noise_volatile(std::size_t n) {
  std::vector<std::uint32_t> v(n);
  volatile std::uint32_t* p = v.data();
  for (std::size_t i = 0; i < n; ++i) p[i] = static_cast<std::uint32_t>(i) * 2654435761u;
}
int main(int argc, char**) { noise_plain(1u << 20); noise_volatile(1u << 20); return argc - 1; }
