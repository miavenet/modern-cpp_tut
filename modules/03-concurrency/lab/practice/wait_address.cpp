// wait_address.cpp - a worker waits for a one-shot value to change from 0 to 7; the producer sleeps 50 ms, stores 7 and notifies.
// Linux only. Build int and char variants (-DSMALL selects char) and trace them with wait_address.sh:
//   bash modules/03-concurrency/lab/practice/wait_address.sh
// Prints OBJECT (the atomic's address) and SIZE on stderr, observed=<value> on stdout.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#ifdef SMALL
using Word=char;
#else
using Word=int;
#endif
alignas(64) std::atomic<Word> value{0};
std::atomic<bool> entered{false};
int main() {
  std::fprintf(stderr,"OBJECT=%p SIZE=%zu\n",static_cast<void*>(&value),sizeof(value));
  std::thread worker([] {
    entered.store(true,std::memory_order_release);
    value.wait(Word{0},std::memory_order_acquire);
    std::printf("observed=%d\n",int(value.load()));
  });
  while (!entered.load(std::memory_order_acquire)) std::this_thread::yield();
  // Gives parking a chance; a trace, not the sleep, must establish parking.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  value.store(Word{7},std::memory_order_release);
  value.notify_one();
  worker.join();
}
