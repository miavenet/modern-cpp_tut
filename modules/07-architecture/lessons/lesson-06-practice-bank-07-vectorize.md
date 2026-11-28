# Lesson 06 — Practice Bank, part 7 of 11 (optional): Does This Loop Vectorize?

**Time: about 28 minutes (estimate)** for 3 cards (8 + 10 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: byte loops, a prefix sum, and a loop that can return early.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB10 · Apply
Q: `pack.cpp` has three loops over a byte buffer. `upper` writes an upper-cased copy: input byte `i` always goes to output byte `i`. `strip_spaces` copies every byte that is not a space: `if (in[i] != ' ') out[m++] = in[i];`. `strip_spaces_nobranch` does the same without an `if`: `out[m] = in[i]; m += (in[i] != ' ');`. Before you compile, write down which of the three the compiler will vectorize at `-O3`. Then ask the compiler instead of reading assembly (`pack_restrict.cpp` is the same with `__restrict` on both pointers):
   ```sh
   clang++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
   g++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -fopt-info-vec-optimized -fopt-info-vec-missed
   clang++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack_restrict.cpp -o /dev/null -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
   ```
   Answer format: which loop is vectorized, the refusal for the other two, then what the obstacle is.
<details><summary>Answer</summary>
Key:

  - Only `upper` is vectorized. Apple clang 21 reports "vectorized loop (vectorization width: 16, interleaved count: 4)" for its loop. GCC 13.3 reports "loop vectorized using 16 byte vectors".
  - Both compaction loops (copying only the kept elements so they sit next to each other) are refused. Apple clang 21 and clang 18: "loop not vectorized: value that could not be identified as reduction is used outside the loop". GCC 13.3: "not vectorized: control flow in loop" for the `if` form and "data ref analysis failed" for the no-branch form.
  - Removing the `if` did not help. The obstacle is not the branch. It is that the output position `m` depends on every earlier element, so the compiler cannot say where element `i` will be written.
  - It is not an aliasing problem either: with `__restrict` on both pointers clang still gives the same "could not be identified as reduction" refusal for both loops. Only the extra "cannot identify array bounds" remark goes away.
  - A wider instruction set did not change it on these compilers: GCC 13.3 with `-march=armv8.2-a+sve` and clang 18 with `-march=armv9-a` still vectorize only `upper`.
  - Habit: the vectorizer report is a one-line command. Run it before you claim a loop "is SIMD" or "cannot be SIMD".
  - Holds for / re-check: Apple clang 21.0.0 (arm64) and GCC 13.3.0 and clang 18.1.3 (aarch64 Linux container), at `-O3`. Not checked on x86. A newer compiler may learn to vectorize this loop shape; the wording of the remarks also changes between versions. Re-check by running the commands above on your compiler and target flags.
</details>

### Card M7-PB11 · Apply
Q: `levels.cpp` has two loops: `add_fee` (can be vectorized) and `running_total` (not vectorized by the tested compilers: a loop-carried prefix-sum dependence, each element needs the one before). You ask for the vectorizer report at `-O0`, `-O1`, `-Os`, `-O2` and `-O3`. Predict for GCC and for clang: at which levels is the report empty, and does an empty report mean "nothing to complain about"?
   ```sh
   bash modules/07-architecture/lab/practice/levels_report.sh clang++
   bash modules/07-architecture/lab/practice/levels_report.sh g++
   ```
   Answer format: the empty levels per compiler, what `add_fee` gets at the other levels, then the method.
<details><summary>Answer</summary>
Key:

  - GCC 13.3: 0 report lines at `-O0`, `-O1` and `-Os`. Both loops are scalar there. The report is empty because the vectorizer did not run, not because the loops are fine.
  - GCC 13.3 at `-O2`: `add_fee` is `missed` ("Loop costings not worthwhile"). At `-O3`: `optimized: loop vectorized`. `running_total` is `missed` at both, with a dependence reason.
  - clang (Apple 21 and 18.1.3): 0 lines at `-O0`. At `-O1`, one bare `loop not vectorized` per loop with no reason. At `-Os`, reasons appear and `add_fee` is declined by the cost model. At `-O2` and `-O3`, `add_fee` is vectorized.
  - Method: keep a loop you know vectorizes in the same file as a control. If the control gets no line, the report channel is dead at this level and silence tells you nothing.
  - Ask for the report with the flags you ship. A report from another level describes other code.
  - Holds for / re-check: `running_total` being declined is a fact about these compilers, not a ban: unsigned addition is associative, and a hand-written SIMD scan (shifts plus a carry between chunks) matched the scalar loop on 1030 cases (a counterexample, not a claim any compiler does this). The line counts are for these three compilers on arm64/aarch64. Which passes run at each level is a compiler-version decision and has changed before, so re-run the script after an upgrade. Not checked on x86.
</details>

### Card M7-PB12 · Apply
Q: Your team wiki says: "A loop that can return in the middle never vectorizes." `search_fixed.cpp` has two search-only loops with no stores: one takes a pointer and a run-time length, the other a reference to a `std::array` of 1024 elements. Name at least two things that could make the wiki line wrong for a given build. Then test it with every compiler you have:
   ```sh
   bash modules/07-architecture/lab/practice/search_report.sh clang++
   bash modules/07-architecture/lab/practice/search_report.sh g++ O3
   ```
   Answer format: two or more things, what each compiler reported, then what the wiki line needs.
<details><summary>Answer</summary>
Key:

  - Examples; others are also correct (target ISA, optimization level or flags). Compiler version: Apple clang 21 reports `vectorized loop (vectorization width: 4, ...)` for the `std::array` version. clang 18.1.3 and GCC 13.3 (`-O3`) leave both versions scalar.
  - Loop shape, i.e. what the compiler knows about the data. In the same Apple clang 21 build the pointer version stays scalar: `Cannot vectorize potentially faulting early exit loop`. A vector read past the exit point could touch memory the scalar loop never reads; with a fixed-size array the compiler knows all of it is readable.
  - GCC's release notes for version 14 announce vectorization of loops with early breaks, at first only when the data size is known at compile time or the alignment can be shown (gcc.gnu.org/gcc-14/changes.html; GCC 14 was not run here).
  - So the wiki line needs a compiler version, a target, and a loop shape next to it, and a command that re-checks it.
  - Holds for / re-check: checked on Apple clang 21.0.0 (arm64), clang 18.1.3 and GCC 13.3.0 (aarch64 Linux), at the flags in the script. Not checked on x86 or on newer GCC. Record the exact compiler string (`--version` output) with a result. Re-check on every compiler upgrade.
</details>

---

## Close

Next: part 8, `lesson-06-practice-bank-08-vector-reports.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
