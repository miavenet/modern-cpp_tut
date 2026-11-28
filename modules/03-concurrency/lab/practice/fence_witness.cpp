// fence_witness.cpp - one-shot publication with a release fence before the token store; two readers: acquire fence after the token load (receive) and before it (receive_early).
// Run the correctly ordered protocol (32 one-shot rounds, writer and reader threads) from the tutorial root:
//   clang++ -std=c++20 -O2 -Wall -Wextra -DRUN modules/03-concurrency/lab/practice/fence_witness.cpp -o "${TMPDIR:-/tmp}/fence_witness" && "${TMPDIR:-/tmp}/fence_witness"
// Arm64 listing: clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/fence_witness.cpp -o -
// x86-64 listing: paste this file into Compiler Explorer (godbolt.org), x86-64 clang 20.1.0, flags -std=c++20 -O2 -Wall -Wextra.
#include <atomic>
#include <cstdio>
#include <thread>
std::atomic<int> payload{0}, token{0};
extern "C" void publish() {
  payload.store(83,std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  token.store(1,std::memory_order_relaxed);
}
extern "C" int receive() {
  if (token.load(std::memory_order_relaxed)!=1) return -1;
  std::atomic_thread_fence(std::memory_order_acquire);
  return payload.load(std::memory_order_relaxed);
}
extern "C" int receive_early() {
  std::atomic_thread_fence(std::memory_order_acquire);
  if (token.load(std::memory_order_relaxed)!=1) return -1;
  return payload.load(std::memory_order_relaxed);
}
#ifdef RUN
int main() {
  unsigned seen=0, stale=0;
  for (int i=0;i<32;++i) {
    payload.store(0); token.store(0);
    int r=-1;
    std::thread writer(publish), reader([&] { r=receive(); });
    writer.join(); reader.join();
    seen += r!=-1; stale += r!=-1 && r!=83;
  }
  std::printf("attempts=32 observed_token=%u stale=%u\n",seen,stale);
  return stale!=0;
}
#endif
