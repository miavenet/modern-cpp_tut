// stream_publish.cpp - three ways to publish a flag after a non-temporal store; which one emits sfence? (x86-64 only)
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   x86-64 clang on Compiler Explorer, flags: -std=c++20 -O2 -Wall -Wextra
#include <atomic>
#include <immintrin.h>
std::atomic<int> flag;
void pub_release_fence(int* d, int v, std::atomic<int>* f) {
  __builtin_nontemporal_store(v, d);
  std::atomic_thread_fence(std::memory_order_release);
  f->store(1, std::memory_order_relaxed);
}
void pub_release_store(int* d, int v, std::atomic<int>* f) {
  __builtin_nontemporal_store(v, d);
  f->store(1, std::memory_order_release);
}
void pub_sfence(int* d, int v, std::atomic<int>* f) {
  __builtin_nontemporal_store(v, d);
  _mm_sfence();
  f->store(1, std::memory_order_release);
}
