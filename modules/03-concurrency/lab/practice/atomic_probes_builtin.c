/* atomic_probes_builtin.c - the four probes with compiler builtins only (no headers),
   so one clang can emit code for other targets. Compile from the tutorial root:
     clang -O2 -S -ffreestanding --target=x86_64-linux-gnu modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
     clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
     clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8.1-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -  */
#include <stdint.h>
_Bool probe_load_acq(const uint64_t* a, uint64_t k) { return __atomic_load_n(a, __ATOMIC_ACQUIRE) == k; }
_Bool probe_load_seq(const uint64_t* a, uint64_t k) { return __atomic_load_n(a, __ATOMIC_SEQ_CST) == k; }
_Bool probe_cas_relaxed(uint64_t* a, uint64_t k) {
  uint64_t e = k;
  return __atomic_compare_exchange_n(a, &e, k, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
_Bool probe_cas_seq(uint64_t* a, uint64_t k) {
  uint64_t e = k;
  return __atomic_compare_exchange_n(a, &e, k, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
