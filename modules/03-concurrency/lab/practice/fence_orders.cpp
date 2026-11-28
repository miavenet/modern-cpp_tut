// fence_orders.cpp - what three thread fences, an acquire load, a release store and a seq_cst store compile to.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/fence_orders.cpp -o -
// For x86-64, POWER and RISC-V, paste this file into Compiler Explorer (godbolt.org) with -O2 -std=c++20.
#include <atomic>
std::atomic<int> g_flag;
void thr_acquire()    { std::atomic_thread_fence(std::memory_order_acquire); }
void thr_release()    { std::atomic_thread_fence(std::memory_order_release); }
void thr_seq_cst()    { std::atomic_thread_fence(std::memory_order_seq_cst); }
int  load_acquire()   { return g_flag.load(std::memory_order_acquire); }
void store_release(int v) { g_flag.store(v, std::memory_order_release); }
void store_seq_cst(int v) { g_flag.store(v, std::memory_order_seq_cst); }
