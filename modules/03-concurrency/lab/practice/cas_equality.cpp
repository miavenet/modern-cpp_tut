// cas_equality.cpp - numeric == against strong compare-exchange for a stored float: negative vs positive zero, and one quiet NaN bit pattern on both sides.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/cas_equality.cpp -o "${TMPDIR:-/tmp}/cas_equality" && "${TMPDIR:-/tmp}/cas_equality"
// One thread, no fast-math. Prints key_check=pass and exits 0 when both probes match the expected results, else FAIL and exit 1.
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
static_assert(std::numeric_limits<float>::digits==24);
bool probe(const char* name, std::uint32_t stored, std::uint32_t wanted, bool want_equal, bool want_cas) {
  const float x=std::bit_cast<float>(stored), y=std::bit_cast<float>(wanted);
  std::atomic<float> a{x};
  const bool equal=a.load(std::memory_order_relaxed)==y;
  float expected=y;
  const bool cas=a.compare_exchange_strong(expected,y,std::memory_order_relaxed);
  std::printf("%s numeric=%d cas=%d expected=%08x final=%08x\n",name,equal,cas,
    static_cast<unsigned>(std::bit_cast<std::uint32_t>(expected)),
    static_cast<unsigned>(std::bit_cast<std::uint32_t>(a.load())));
  return equal==want_equal && cas==want_cas;
}
int main() {
  const bool zero_ok=probe("signed_zero",0x80000000u,0x00000000u,true,false);
  const bool nan_ok=probe("same_nan",0x7fc00031u,0x7fc00031u,false,true);
  const bool ok=zero_ok && nan_ok;
  std::printf("key_check=%s\n", ok ? "pass" : "FAIL");
  return ok ? 0 : 1;
}
