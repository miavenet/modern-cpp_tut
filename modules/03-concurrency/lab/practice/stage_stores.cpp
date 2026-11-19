// stage_stores.cpp - three stores in a row to one variable: plain int, volatile int, relaxed atomic.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O1 -Wall -Wextra -S modules/03-concurrency/lab/practice/stage_stores.cpp -o -   (then -O2, then g++ the same way)
// Three "progress" stores to the same variable, three ways.
#include <atomic>
int plain_stage;
volatile int vol_stage;
std::atomic<int> atom_stage;

void step_plain()  { plain_stage = 1; plain_stage = 2; plain_stage = 3; }
void step_vol()    { vol_stage = 1;   vol_stage = 2;   vol_stage = 3; }
void step_atomic() {
    atom_stage.store(1, std::memory_order_relaxed);
    atom_stage.store(2, std::memory_order_relaxed);
    atom_stage.store(3, std::memory_order_relaxed);
}
