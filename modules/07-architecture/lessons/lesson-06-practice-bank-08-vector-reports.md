# Lesson 06 — Practice Bank, part 8 of 11 (optional): Asking the Compiler What It Did

**Time: about 30 minutes (estimate)** for 4 cards (10 + 10 + 5 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: reading listings, comparing compilers, and optimization remarks for vectorizing and unrolling.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB13 · Spot the trap
Q: A teammate decides whether a loop was vectorized by counting listing lines that mention `xmm`, `ymm` or `zmm`. For `total()` in `scalar_fp.cpp` (a plain sum of `double`s, compiled for x86-64 with `-O2 -mavx2`) the count is above zero, so they report "vectorized". What is wrong with the test, and what would you count instead?
   ```sh
   bash modules/07-architecture/lab/practice/scalar_fp.sh clang++
   ```
   Answer format: why the test is wrong, what the counts show, then what to count instead.
<details><summary>Answer</summary>
Key:

  - On x86-64 by default all `double` arithmetic uses the xmm registers, scalar code included (SSE2 is the baseline; x87 code is possible, for example for `long double` or with `-mfpmath=387`). So "mentions xmm" does not mean SIMD work.
  - Measured: `total` has 12 lines naming xmm, 0 naming ymm, 9 scalar-double instructions (`vaddsd`) and 0 packed arithmetic ones. It is scalar. The compiler says so too: `loop not vectorized: cannot prove it is safe to reorder floating-point operations`.
  - The control `twice()` has packed arithmetic (5 packed-double arithmetic instructions on Apple clang 21, 4 on clang 18) and ymm registers.
  - Beware a name-pattern census: counting every mnemonic ending in `ps`/`pd` gives 3 hits on the scalar `total` (`vxorpd` x2, `vxorps`: register zeroing), so it would also report "vectorized". Count packed arithmetic by name (`vaddpd`, `vmulpd`, ... or integer `vp...` on ymm/zmm) and whole-vector stores. Always run the census on one loop you know is scalar and one you know is vectorized, to test the census itself.
  - Holds for / re-check: x86-64 assembly produced by cross-compiling with Apple clang 21 and clang 18.1.3 (same result on both); not run on GCC or on an x86 machine. The register-name trap is specific to x86; on AArch64 the names differ, but the rule "test your census on a known-scalar and a known-vector loop" still applies. The result for `total()` depends on the target: both clangs report it as vectorized for `--target=aarch64-linux-gnu`, so re-run for your target.
</details>

### Card M7-PB14 · Apply
Q: Compile `count_above(const int* a, int n, int t)` at `-O2 -std=c++20` with x86-64 GCC 13.3 and x86-64 clang 18.1 on Compiler Explorer, and locally with Apple clang (arm64) and GCC 13.3 (aarch64). Which listings contain SIMD compare instructions? Then try GCC with `-O3`.
   ```sh
   clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -
   g++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -
   ```
   Answer format: one line per compiler at `-O2`, one for GCC `-O3`, then the lesson.
<details><summary>Answer</summary>
Key:

  - At `-O2`, clang vectorizes: x86-64 clang 18.1 uses `xmm` registers (`pcmpgtd`); Apple clang arm64 uses `cmgt` vector compares.
  - At `-O2`, GCC 13.3 does not: one element per iteration (x86: `cmpl`/`setl`/`addl`, no branch on the element). Same on aarch64.
  - With `-O3`, GCC vectorizes too (the x86 listing has `xmm`; aarch64 has `cmgt`/`.4s`).
  - Lesson: `-O2` is a per-compiler bundle of passes: clang's includes the loop vectorizer; GCC 13's uses the very-cheap cost model and left this loop scalar. Read the listing, not the flag name.
  - Holds for / re-check: GCC 13.3 and clang 18.1 (x86-64, Compiler Explorer), Apple clang 21 and GCC 13.3 (aarch64, local). GCC's `-O2` vectorizer rules changed in GCC 12 (gcc.gnu.org/gcc-12/changes.html: vectorization at `-O2` with the very-cheap cost model) and may change again; re-check per release.
</details>

### Card M7-PB16 · Apply
Q: A teammate tested a 12-step hash loop (`h = h * 31u + p[i]` for `i` from 0 to 11) with GCC at `-O2`, saw a real loop, and wrote in the wiki: "GCC keeps short fixed loops rolled; only clang unrolls them." What setting change would break this rule, and how do you show it in two commands? (A backward branch, one whose target is earlier in the listing, means a loop in this small function.)
   ```sh
   g++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/hashloop.cpp -o -
   g++ -std=c++20 -O3 -S modules/07-architecture/lab/practice/hashloop.cpp -o -
   ```
   Answer format: the setting, the two counts, then what a rule about unrolling must name.
<details><summary>Answer</summary>
Key:

  - The `-O` level. GCC 13.3 at `-O2` kept the loop (1 backward branch); at `-O3` it unrolled completely (0 backward branches). Apple clang unrolled completely at `-O2` and kept the loop at `-O1`.
  - Show it: build the same file with `-O2 -S` and `-O3 -S`, and count backward branches in each listing. Do not count a fixed list of conditional-branch mnemonics: they differ per ISA and per compiler.
  - So a rule about unrolling must name compiler, version, `-O` level and target, and be re-run when any of them changes.
  - Holds for / re-check: GCC 13.3 aarch64 and Apple clang 21 arm64, one source file. Re-check on your toolchain; release notes for loop passes are the place to look when the shape changes.
</details>

### Card M7-PB15 · Recall
Q: You want each compiler to tell you, in words, whether it unrolled a loop (copied the body so fewer branches run), without reading assembly. The commands below ask clang (`-Rpass=loop-unroll`) and GCC (`-fopt-info-loop`) for their remarks. Predict what each prints for a 12-iteration loop it unrolls completely, including the iteration count each one reports, then run them. On reviews, recall both flags and both messages.
   ```sh
   clang++ -std=c++20 -O2 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -Rpass=loop-unroll
   g++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -fopt-info-loop
   ```
   Answer format: one message (with its flag) per compiler, then how far to trust remarks.
<details><summary>Answer</summary>
Key:

  - clang: `-Rpass=loop-unroll` prints `remark: completely unrolled loop with 12 iterations [-Rpass=loop-unroll]`. Add `-Rpass-missed=loop-unroll` to see refusals.
  - GCC: `-fopt-info-loop` prints `optimized: loop with 11 iterations completely unrolled`. GCC counts latch runs, so it says 11 for a 12-iteration loop: read the number with care.
  - Remarks are a fast first filter; confirm with the listing, because other passes (vectorizer, later cleanups) can still change the shape.
  - Holds for / re-check: Apple clang 21 (Mac), GCC 13.3 (Linux aarch64). Remark wording and counting differ between releases. Re-check by running the flags on a loop whose trip count you know.
</details>

---

## Close

Next: part 9, `lesson-06-practice-bank-09-counters-models.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
