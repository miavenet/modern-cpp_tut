# Lesson 06 — Practice Bank, part 11 of 11 (optional): Concurrency Checks and the Environment

**Time: about 13 minutes (estimate)** for 3 cards (3 + 6 + 4 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: a stress test for torn reads, ThreadSanitizer on a version counter, and interrupt counts in a container.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB20 · Apply
Q: Before timing any concurrent copy scheme, build a self-checking stress test that can expose inconsistent snapshots (torn reads: a copy that mixes parts of two different writes). In `torn_detect.cpp` the writer always stores the same value into all eight words (each word a `std::atomic<long>`, written relaxed). The reader copies the eight words and counts snapshots whose words differ. Mode `none` copies with no protocol; mode `seq` wraps the copy in a version-counter check. Predict the torn count for each mode (zero or non-zero), then run three times:
   ```sh
   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/torn_detect.cpp -o "${TMPDIR:-/tmp}/torn_detect" && for r in 1 2 3; do "${TMPDIR:-/tmp}/torn_detect" none && "${TMPDIR:-/tmp}/torn_detect" seq; done
   ```
   Answer format: zero or non-zero per mode, what the invariant costs, then what a passing `seq` run proves.
<details><summary>Answer</summary>
Key:

  - `none`: non-zero torn count, and `seq`: torn = 0, in all 3 of 3 runs here. Neither the exact counts nor "most" snapshots is guaranteed on another run or machine; scheduling can give an unprotected run with no observed tear.
  - The "all words equal" invariant needs no separate reference copy, but it is not free: the check costs several compares and branches per snapshot.
  - A deliberately broken control (`none`) shows the checker can see tearing in the cases run. A passing run of `seq` supplements a memory-model argument for the protocol; it does not prove the protocol correct. Each word is atomic, so the unprotected mode is torn but not undefined behaviour.
  - Holds for / re-check: direction (non-zero vs zero) on M1 Max with Apple clang 21, 3 runs. If `none` ever shows 0, make the copy longer or the writer faster before trusting the test. Re-check on each target.
</details>

### Card M7-PB23 · Recall
Q: You want to test the claim "no interrupts reached CPU 0 during my run" by diffing `/proc/interrupts` before and after, inside a Docker container. The diff is empty. What else must you check before you accept the claim? Run these in a default container:
   ```sh
   ls -l /proc/interrupts /dev/null; wc -l < /proc/interrupts; grep interrupts /proc/self/mountinfo
   ```
   Answer format: what the three commands show, then the rule.
<details><summary>Answer</summary>
Key:

  - In this container `/proc/interrupts` is a character device 1,3, the same as `/dev/null`, has 0 lines, and `mountinfo` shows `/null` mounted over it. The runtime masks the file. An empty diff means "no data", not "no interrupts".
  - `/proc/stat` is still readable there and its `intr` line has totals, but not the per-CPU split you need.
  - Rule: before trusting a counter, check that it exists and moves (read it twice around something that must change it). Measure isolation questions on the host, not in a container.
  - Holds for / re-check: Docker with default flags (kernel 7.0.12-linuxkit). Other runtimes or privileged containers may expose the real file. The masking was observed, not looked up in Docker's documentation.
</details>

### Card M7-PB24 · Apply
Q: `seq_tsan.cpp` has one writer and one reader using a version counter around a 4-word struct. With `-DCOPY=0` both sides copy the words with plain loads and stores; with `-DCOPY=1` every word goes through `std::atomic_ref<long>` relaxed loads/stores. A teammate builds `COPY=0` with Apple clang ThreadSanitizer (TSan), sees no report in three runs, and says "TSan says it is race-free". Build both variants with TSan on every toolchain you have and run each three times:
   ```sh
   for c in 0 1; do clang++ -std=c++20 -O1 -g -fsanitize=thread -Wall -Wextra -DCOPY=$c modules/07-architecture/lab/practice/seq_tsan.cpp -o "${TMPDIR:-/tmp}/seq_tsan$c" && for r in 1 2 3; do "${TMPDIR:-/tmp}/seq_tsan$c"; done; done
   ```
   (Repeat with `g++` and `clang++` on Linux.) What do you see, and what do you conclude?
   Answer format: one line per toolchain, the GCC compile-time warning, then the conclusion.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 (macOS arm64): no report for either variant in 3 of 3 runs, although the reader retried hundreds of thousands of times, so the threads did overlap.
  - GCC 13.3 and clang 18.1.3 (Linux aarch64): `COPY=0` gives "WARNING: ThreadSanitizer: data race" in 3 of 3 runs each; `COPY=1` gives none.
  - GCC also prints at compile time: "'atomic_thread_fence' is not supported with '-fsanitize=thread' [-Wtsan]". The detector does not model fences, so its verdict on fence-based code is not reliable either way.
  - Conclusion: a quiet sanitizer is not proof. The plain copy is a data race by the standard ([intro.races] p21, C++23 draft N4950) no matter what one tool says. Run more than one toolchain, and prefer atomic (relaxed) word copies, which are race-free by construction.
  - Holds for / re-check: results for the three toolchains named, on arm64. Sanitizer runtimes change between releases (fence support may be added); re-check by running both variants on every new toolchain, and keep the `COPY=1` variant as a "should be quiet" control.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
