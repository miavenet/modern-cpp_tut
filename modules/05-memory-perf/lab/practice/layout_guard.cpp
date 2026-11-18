// layout_guard.cpp - an order-safe line-separation guard, tested on four structs (one -D flag per struct).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   for s in T_ALIGNED_STRUCT T_ALIGNED_MEMBERS T_REORDERED_STRUCT T_REORDERED_MEMBERS OLD_GUARD_REORDERED; do echo "== $s"; clang++ -std=c++20 -Wall -Wextra -D$s -fsyntax-only modules/05-memory-perf/lab/practice/layout_guard.cpp && echo compiles; done
// Order-safe line-separation guard for two counters written by different threads.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
constexpr std::size_t kLine = 128;   // one named constant
constexpr bool on_different_lines(std::size_t a, std::size_t b) { return a / kLine != b / kLine; }

struct alignas(128) AlignedStruct {            // fills at 0, rejects at 8
  std::atomic<std::uint64_t> fills; std::atomic<std::uint64_t> rejects; };
struct AlignedMembers {                         // fills at 0, rejects at 128
  alignas(128) std::atomic<std::uint64_t> fills; alignas(128) std::atomic<std::uint64_t> rejects; };
struct alignas(128) ReorderedStruct {           // rejects at 0, fills at 8 (swapped)
  std::atomic<std::uint64_t> rejects; std::atomic<std::uint64_t> fills; };
struct ReorderedMembers {                       // rejects at 0, fills at 128 (swapped)
  alignas(128) std::atomic<std::uint64_t> rejects; alignas(128) std::atomic<std::uint64_t> fills; };

#define CHECK(T) \
  static_assert(std::is_standard_layout_v<T>, #T " must be standard-layout for offsetof"); \
  static_assert(alignof(T) % kLine == 0, #T " is not line-aligned"); \
  static_assert(on_different_lines(offsetof(T, fills), offsetof(T, rejects)), #T ": fills and rejects share a line")

#ifdef T_ALIGNED_MEMBERS
CHECK(AlignedMembers);
#endif
#ifdef T_ALIGNED_STRUCT
CHECK(AlignedStruct);
#endif
#ifdef T_REORDERED_STRUCT
CHECK(ReorderedStruct);
#endif
#ifdef T_REORDERED_MEMBERS
CHECK(ReorderedMembers);
#endif
#ifdef OLD_GUARD_REORDERED   // the original subtraction guard, applied to the swapped struct
static_assert(offsetof(ReorderedStruct, rejects) - offsetof(ReorderedStruct, fills) >= 128, "old guard");
#endif
int main() {}
