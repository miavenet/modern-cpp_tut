// layout.cpp - three layouts of two counters written by two threads.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -Xclang -fdump-record-layouts -fsyntax-only modules/05-memory-perf/lab/practice/layout.cpp
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
// Two counters written by two different threads.
struct Stats {
  std::atomic<std::uint64_t> fills;     // written by thread A
  std::atomic<std::uint64_t> rejects;   // written by thread B
};
// Attempt 1: align the whole struct.
struct alignas(128) StatsAlignedStruct {
  std::atomic<std::uint64_t> fills;
  std::atomic<std::uint64_t> rejects;
};
// Attempt 2: align each member.
struct StatsAlignedMembers {
  alignas(128) std::atomic<std::uint64_t> fills;
  alignas(128) std::atomic<std::uint64_t> rejects;
};
Stats s0; StatsAlignedStruct s1; StatsAlignedMembers s2;
#ifdef GUARD
static_assert(offsetof(StatsAlignedStruct, rejects) - offsetof(StatsAlignedStruct, fills) >= 128,
              "fills and rejects can share a cache line");
#endif
int main() { return 0; }
