// lock_transfer.cpp - a counting Gate under unique_lock: a discarded std::move cast, a move construction in an inner scope, and both scope exits.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/lock_transfer.cpp -o "${TMPDIR:-/tmp}/lock_transfer" && "${TMPDIR:-/tmp}/lock_transfer"
// Single thread; Gate only counts lock() and unlock() calls. Exits 1 unless the gate ends with one lock and one unlock.
#include <mutex>
#include <utility>
#include <cstdio>
struct Gate {
  int locks=0, unlocks=0;
  void lock() { ++locks; }
  void unlock() { ++unlocks; }
};
int main() {
  Gate g;
  {
    std::unique_lock<Gate> first(g);
    (void)std::move(first);
    std::printf("cast first=%d locks=%d unlocks=%d\n",first.owns_lock(),g.locks,g.unlocks);
    {
      std::unique_lock<Gate> second(std::move(first));
      std::printf("construct first=%d second=%d locks=%d unlocks=%d\n",
        first.owns_lock(),second.owns_lock(),g.locks,g.unlocks);
    }
    std::printf("inner_exit locks=%d unlocks=%d\n",g.locks,g.unlocks);
  }
  std::printf("outer_exit locks=%d unlocks=%d\n",g.locks,g.unlocks);
  return !(g.locks==1 && g.unlocks==1);
}
