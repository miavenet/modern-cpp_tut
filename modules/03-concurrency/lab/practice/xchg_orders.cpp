// xchg_orders.cpp - what instruction a lock-acquire exchange, a release / seq_cst store and a relaxed load become, per memory order and target.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
//   clang++ -std=c++20 -O2 -mcpu=generic -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
//   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
// What instruction does a lock-acquire exchange become, per memory order and target?
#include <atomic>
std::atomic<bool> f;
bool acq_relaxed() { return f.exchange(true, std::memory_order_relaxed); }
bool acq_acquire() { return f.exchange(true, std::memory_order_acquire); }
bool acq_seqcst()  { return f.exchange(true, std::memory_order_seq_cst); }
void rel_release() { f.store(false, std::memory_order_release); }
void rel_seqcst()  { f.store(false, std::memory_order_seq_cst); }
bool peek_relaxed(){ return f.load(std::memory_order_relaxed); }
