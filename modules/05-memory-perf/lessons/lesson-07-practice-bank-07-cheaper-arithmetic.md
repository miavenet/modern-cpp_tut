# Lesson 07 — Practice Bank, part 7 of 9 (optional): Cheaper-Arithmetic Claims

**Time: about 20 minutes (estimate)** for 2 cards (10 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: mask against modulo, and a branch-free filter, each checked rather than assumed.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

### Card M5-PB3 · Apply
Q: A code review comment says "use `x & (n-1)` instead of `x % n` for a power-of-two `n`, it is cheaper". You want to know if that holds for your compiler and CPU. Describe the check, say what you would count, and predict the result for four cases: `x` unsigned or signed, `n` a compile-time constant or a runtime value. Then run the check:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/modmask.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/modmask.cpp modules/05-memory-perf/lab/practice/modmask_neg.cpp -o "${TMPDIR:-/tmp}/modmask_neg" && "${TMPDIR:-/tmp}/modmask_neg"
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/layout_ldp.cpp -o -
   ```
   Answer format: the method in one line, then one line per case.
<details><summary>Answer</summary>
Key:

  - Write each case as a tiny `extern "C"` function, compile with `-O2 -S`, and read the listing. Count the instructions on the path that runs, not the source lines.
  - Here (Apple clang 21, arm64): unsigned `x`, constant `n = 1024`: both compile to the same single `and x0, x0, #0x3ff`. The rewrite gains nothing; the compiler already does it.
  - Signed `x`, constant `n`: `x % 1024` becomes `negs`, two `and`, `csneg` (4 instructions) because the result must keep the sign of `x`. `x & 1023` is one `and`, but it gives a different answer for negative `x` (run here: `-5 % 1024` = -5, `-5 & 1023` = 1019). So the "optimisation" can change behaviour; make the type unsigned instead.
  - Unsigned `x`, runtime `n`: `x % n` is `udiv` + `msub`; `x & (n-1)` is `sub` + `and`. The mask avoids the divide instruction; time it before claiming a speed-up. It is only correct if the caller guarantees `n` is a power of two.
  - Signed `x`, runtime `n`: `x % n` is `sdiv` + `msub`; `x & (n-1)` is `sub` + `and`. Same trade as the unsigned case, plus the sign problem: the results differ for negative `x` (run here with `n` passed at run time: -5 vs 1019).
  - Counts also shift with unrelated details. Example: two loads of adjacent struct fields became one `ldp` here; moving one field 16 bytes away gave two `ldr`. Compare whole functions in their real context before believing a one-instruction claim. Then time it in a loop if it matters.
  - Holds for / re-check: these listings are specific to this compiler version, arm64 and `-O2`. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 gave the same four shapes; x86-64 was not checked. Repeat the four cases there, and re-read the listing after compiler upgrades.
</details>

### Card M5-PB19 · Spot the trap
Q: A colleague replaces a branchy filter with a branch-free one. The job: keep the odd order ids.
   ```cpp
   std::size_t keep_odd_nobranch(const std::uint32_t* in, std::size_t n, std::uint32_t* out) {
       std::size_t m = 0;
       for (std::size_t i = 0; i < n; ++i) {
           out[m] = in[i];
           m += in[i] & 1u;
       }
       return m;
   }
   ```
   The caller counts the odd ids first and allocates `out` with exactly that many slots. The unit test passes. What is wrong, why can a test miss it, and how do you catch it? Build `slack.cpp` and run all four cases (`b` = branchy, `n` = no branch, `k` = input ordered so the kept ids come last):
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/slack.cpp -o "${TMPDIR:-/tmp}/slack" && "${TMPDIR:-/tmp}/slack" b && "${TMPDIR:-/tmp}/slack" b k && "${TMPDIR:-/tmp}/slack" n k && "${TMPDIR:-/tmp}/slack" n
   ```
   Answer format: the bug, what the detector says, why a test passes, then the fix.
<details><summary>Answer</summary>
Key:

  - The branch-free form writes `out[m]` for every input element, kept or not. A rejected element is written to the next free slot and overwritten later, or never.
  - If a rejected element arrives after the output is already full, that write lands one slot past the end. With 4 kept ids and a 4-slot buffer, input `11 13 15 17 20 22` writes `out[4]`.
  - AddressSanitizer (ASan, a compiler mode that instruments the memory accesses that survive optimization) reports it: "heap-buffer-overflow ... WRITE of size 4 ... located 0 bytes after 16-byte region", in `keep_odd_nobranch`. Seen with Apple clang 21 and with GCC 13.3 on Linux.
  - The bug depends on the data. The same ids in the order `20 22 11 13 15 17` (last element kept) run clean under ASan, and the branchy version is clean for both orders. So a test with a lucky input passes.
  - Without ASan, on the builds tested (Apple clang 21, GCC 13.3, clang 18 at `-O1`), the bad run prints "kept 4 of 6" and exits 0, so nothing visible fails. It is undefined behaviour, so other outcomes on other builds are possible.
  - **Fix:** a branch-free pack needs spare room in the output. For this scalar form one extra element is enough (checked at `-O1` under ASan with Apple clang 21, 2047 cases: all 0/1 inputs of length 0 to 10 with capacity kept+1 were clean and counted correctly, with the function marked `[[gnu::noinline]]` so its stores could not be optimized away; the control with capacity == kept failed with a heap-buffer-overflow WRITE of size 4). The invariant behind it: at every store m <= the total kept, and a buffer of `n` elements is always safe. A version that stores a whole vector at a time needs more spare room; that is not tested here. Put the requirement in the function's contract and run the tests under ASan with an input that ends in rejected elements.
  - Holds for / re-check: a property of the algorithm, not of a compiler, so it does not expire. The ASan wording is from Apple clang 21.0.0 (arm64) and GCC 13.3.0 (aarch64 Linux). Re-check by running the four commands.
</details>

---

## Close

Next: part 8, `lesson-07-practice-bank-08-float-sums.md` (about 29 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
