# Lesson 06 — Practice Bank, part 2 of 8 (optional): Exchange Orders and Locks

**Time: about 30 minutes (estimate)** for 3 cards (10 + 10 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what each memory order costs on an exchange, a test-and-test-and-set lock, and what `lock_shared()` is built from.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB6 · Apply
Q: `xchg_orders.cpp` has `f.exchange(true, order)` for relaxed, acquire and seq_cst, plus a release and a seq_cst `store(false)` and a relaxed load. Predict the instruction each becomes on (1) Apple arm64, (2) arm64 without LSE, the ARMv8.1 atomic instructions such as `swp` (`-mcpu=generic`), (3) x86-64. Then compile:
   ```sh
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   clang++ -std=c++20 -O2 -mcpu=generic -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   ```
   For each target, which operations (the exchanges, the stores) change instruction when only the memory order changes, and why does that matter when you port a lock benchmark?
   Answer format: one line per target, then why it matters.
<details><summary>Answer</summary>
Key:

  - arm64 (default CPU, LSE): relaxed `swpb`, acquire `swpab`, seq_cst `swpalb`; both stores `stlrb`; relaxed load `ldrb`.
  - arm64 `-mcpu=generic`: a load-exclusive/store-exclusive loop (`ldxrb/stxrb`, `ldaxrb/stxrb`, `ldaxrb/stlxrb`).
  - x86-64: all three exchanges are `xchgb`; the release store is a plain `movb`, the seq_cst store is `xchgb`; the relaxed load is `movzbl`. On x86 an exchange with a memory operand is always locked, whatever order you ask for (Intel SDM, XCHG). GCC 13.3 x86-64 (Compiler Explorer, `-O2`) shows the same pattern.
  - So the order changes different operations on different targets: on arm64 it changes the exchange while both stores are `stlrb`; on x86-64 it changes the store (`movb` vs `xchgb`) while every exchange is `xchgb`. A weak order on the exchange is invisible in the x86 code. Re-test when moving between ISAs.
  - Not shown: the locked `xchg` fixes the hardware order only. The standard still lets the compiler move other accesses around a relaxed exchange; GCC 13.3 did not reorder a two-store test function, so that is a rule of the language, not an observation here.
  - GCC 13 on Ubuntu aarch64 emits a call to a helper (`bl __aarch64_swp1_acq`), not an inline instruction: the toolchain default matters too.
  - Holds for / re-check: Apple clang 21.0.0; GCC 13.3 on Ubuntu aarch64 (outline-atomics default); x86-64 is compile only, nothing x86 was run. Different CPU flags (`-mcpu`, `-march`) change the arm64 output; re-check with `-S`.
</details>

### Card M3-PB7 · Apply
Q: `ttas_lock.cpp` is a test-and-test-and-set (TTAS) lock guarding a plain `long counter`: it spins on a plain atomic load and tries the exchange only when the lock looks free. 4 threads add 20,000 each and the program checks counter == 80,000. Build it two ways: correct orders (`-DOK`: acquire exchange, release store) and all-relaxed (`-DRELAXED`). Run each 5 times at `-O2`, then 3 times under ThreadSanitizer:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DOK modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   clang++ -std=c++20 -O2 -Wall -Wextra -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   clang++ -std=c++20 -O1 -g -fsanitize=thread -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   ```
   Is the counter check enough to accept the lock?
   Answer format: one line per build, then your verdict on the check.
<details><summary>Answer</summary>
Key:

  - Correct orders: PASS in all 5 plain runs; TSan reports 0 data races in 3 of 3 runs.
  - All-relaxed: the plain build FAILed in some runs and PASSed in others (for example 3 FAIL, 2 PASS out of 5). TSan reported a data race on `counter` in 3 of 3 runs (exit 134).
  - So a passing invariant check is necessary but not proof: a wrong lock can pass on a given run or machine. Add a race detector run, which checks the ordering rule, not one outcome.
  - Relaxed exchange/store do not create a happens-before edge between the unlock and the next lock, so the plain `counter` accesses race.
  - Holds for / re-check: Apple clang 21.0.0 on M1 Max (a weakly ordered CPU). How often the relaxed build fails depends on the CPU and timing; on x86 the exchange is always locked (card M3-PB6), so it may fail less often or not at all there (not run here; the compiler may still reorder relaxed code). The TSan result does not depend on the CPU. Re-check: rerun both builds on the target box.
</details>

### Card M3-PB12 · Recall
Q: What is `lock_shared()` built from in two standard libraries, and how do you find out? (a) Compile a function that takes `std::shared_lock` and read the calls in the `-S` output; (b) read the class in the library header; (c) look at the function body that the program really runs (a debugger `disassemble` on a running process works). What did you find for libc++ (Apple clang) and for libstdc++ (GCC 13) on Linux?
   ```sh
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/reader_lock.cpp -o -
   ```
   Answer format: one line per library, then the technique.
<details><summary>Answer</summary>
Key:

  - libc++ (Apple clang 21): `read_value()` calls `std::__shared_mutex_base::lock_shared` / `unlock_shared`. The header shows the base holds a `mutex`, two `condition_variable`s and an `unsigned` state. The shipped `lock_shared` body (disassembled with lldb in a running process) calls `std::mutex::lock()`, loads the state word, stores it back incremented, then calls `std::mutex::unlock()`. Upstream libc++ `llvmorg-22.1.6` `src/shared_mutex.cpp` (byte-identical to `llvmorg-21.1.0`) has the same structure (`unique_lock lk(__mut_)`, `__state_` incremented); the upstream file is corroboration, not proof of which source Apple built.
  - libstdc++ (GCC 13.3, Ubuntu 24.04 aarch64, glibc 2.39): the calls are `pthread_rwlock_rdlock` and `pthread_rwlock_unlock` (header line `return pthread_rwlock_rdlock(__rwlock)`). The glibc 2.39 `pthread_rwlock_rdlock` disassembled with gdb has its first call at +68, after two checks: `__aarch64_ldadd4_acq`, an atomic add on the lock word at its fast path, in line with `nptl/pthread_rwlock_common.c` (glibc 2.39).
  - Technique: the standard says what a class does, not how; the listing, header and the body that runs say how. Check all three on each target library.
  - Holds for / re-check: SDK libc++ `_LIBCPP_VERSION 220106` with Apple clang 21 on macOS 27.0.1 (lldb disassembly of the shipped `libc++.1.dylib`), libstdc++ of GCC 13.3 with glibc 2.39 on Linux aarch64. Do not generalise to all standard libraries: implementations change between releases, so re-check with `-S`, the header and a disassembly for each new toolchain.
</details>

---

## Close

Next: part 3, `lesson-06-practice-bank-03-wait-wake.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
