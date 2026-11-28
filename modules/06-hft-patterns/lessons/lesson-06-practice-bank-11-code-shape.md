# Lesson 06 — Practice Bank, part 11 of 12 (optional): Hot-Path Code Shape

**Time: about 24 minutes (estimate)** for 4 cards (6 + 5 + 6 + 7 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: guaranteed tail calls, choosing a fast path by CPU, and folding known steps.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB24 · Apply
Q: `musttail_feat.cpp` prints nothing; it stores `__has_cpp_attribute(clang::musttail)` and `__has_cpp_attribute(gnu::musttail)` in two globals and has one function whose `return` carries `[[clang::musttail]]` (a Clang attribute that makes the tail call a requirement: compile error if it cannot be done). Predict both values and whether the file compiles with GCC 13, GCC 14, GCC 15 and clang 18/20. Check on Compiler Explorer (x86-64, `-O1 -std=c++20`) and read the globals in the listing; locally:
   ```sh
   clang++ -std=c++20 -O1 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/musttail_feat.cpp -o -
   ```
   What text does GCC print, does it name the attribute, and what one flag makes it a build error?
   Answer format: one line per compiler, then the flag and the habit.
<details><summary>Answer</summary>
Key:

  - GCC 13.3 and 14.2 (x86-64): compiles; both globals are 0 (`.zero 4`); warning `attributes at the beginning of statement are ignored [-Wattributes]`. The text does not say `musttail`. With `-Werror=attributes` it is an error (`cc1plus: some warnings being treated as errors`).
  - GCC 15.3: both values are 1 (`.long 1`), no warning: the attribute is accepted under both spellings.
  - clang 18.1 and 20.1: `clang::musttail` is 1, `gnu::musttail` is 0, no warning.
  - Lesson: an ignored statement attribute is a warning on a line that does not name it. Feature-test with `__has_cpp_attribute(...)` (the value, not just the spelling, differs per compiler) and put `-Werror=attributes` on, or `#error` when the macro is 0, so a build without the guarantee fails instead of silently producing ordinary calls.
  - Holds for / re-check: Compiler Explorer, x86-64: GCC 13.3, 14.2, 15.3, clang 18.1.0 and 20.1.0, compile only, nothing run. Which compilers define which spelling changes between releases; re-run the same two-line test on any new compiler.
</details>

### Card M6-PB25 · Spot the trap
Q: Which of these does Apple clang 21 accept under `[[clang::musttail]]`? (a) `int ok(int a, int b) { [[clang::musttail]] return two(b, a); }` with `int two(int, int)`; (b) `int fewer(int a, int b) { [[clang::musttail]] return one(a + b); }` with `int one(int)`; (c) `int widened(int a, int b) { [[clang::musttail]] return (int)wide(a + b); }` with `long wide(int)`; (d) a function that copies an argument into a local, reads it through a pointer, then `[[clang::musttail]] return two(*p, b);`.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/06-hft-patterns/lab/practice/musttail_rules.cpp -o "${TMPDIR:-/tmp}/musttail_rules.o"
   ```
   Answer format: which compile, the two error messages, then the design rule.
<details><summary>Answer</summary>
Key:

  - (a) and (d) compile.
  - (b) error: "cannot perform a tail call to function 'one' because its signature is incompatible with the calling function", note "different number of parameters (expected 2 but has 1)".
  - (c) error: "'clang::musttail' attribute requires that the return value is the result of a function call" (a cast is extra work after the call).
  - Design rule: give every handler in a tail-call chain the same signature, and return the call's result unchanged.
  - Holds for / re-check: Apple clang 21.0.0, `-std=c++20 -O2 -Wall -Wextra`. Clang's rules for musttail have changed across versions; re-check by compiling the same four functions.
</details>

### Card M6-PB30 · Spot the trap
Q: A library chooses its fast path like this:
   ```cpp
   #ifdef __AVX2__
     use_fast_path();
   #else
     use_portable_path();
   #endif
   ```
   The team says "this picks the right path for whatever CPU the binary lands on." Is that true? Build `modules/06-hft-patterns/lab/practice/avx2_macro.cpp` (prints which branch the preprocessor chose, then calls `__builtin_cpu_supports("avx2")`) on Compiler Explorer with x86-64 gcc, flags `-O2` and then `-O2 -mavx2`, and compare the assembly.
   Answer format: true or false, what each binary contains, what the runtime call does, then the fix.
<details><summary>Answer</summary>
Key:

  - False. `__AVX2__` is set by the build flags, not by the CPU that runs the binary. With `-O2` the binary contains only the string "built without __AVX2__"; with `-O2 -mavx2` it contains only "built with __AVX2__ defined". The other branch is not in the binary at all.
  - `__builtin_cpu_supports("avx2")` is different: the assembly calls `__cpu_indicator_init` and reads `__cpu_model` at run time, so it answers for the CPU it runs on.
  - Consequences: a plain build never takes the fast path, even on a CPU that has AVX2; in a `-mavx2` build the AVX2 branch is selected at compilation, and execution on an incompatible CPU is not guaranteed.
  - Fix: decide at run time once (for example with `__builtin_cpu_supports` at start-up, stored in a function pointer), or build both paths and dispatch. Compile the dispatcher and fallback for the deployment baseline; compile the AVX2 implementation separately with `-mavx2` or a supported per-function target attribute, then dispatch after the runtime feature check. Adding a check to a translation unit globally built with `-mavx2` does not make it portable to non-AVX2 CPUs, because the compiler may emit AVX2 instructions anywhere in it. A compile-time macro only says what you allowed the compiler to use.
  - Holds for / re-check: x86-64 gcc 13.3 on Compiler Explorer (its runs show behaviour on that host only, not on an incompatible CPU). `__builtin_cpu_supports` is a GCC/clang extension for x86 (Arm targets have different names and feature sets). Re-check: grep your release binary's assembly for the string or symbol of each path.
</details>

### Card M6-PB34 · Apply
Q: A drawing tool receives a coordinate `x` after three calibration steps are already known. The steps, in order, are `3*x+2`, then `-2*x+5`, then `4*x-3`, each applied to the previous result. An affine transformation has the form `a*x+b`. Derive one affine transformation that does all three before you open the program. Implement the composition rule in your own small program, then compare with `affine_schedule.cpp`. Predict the outputs for `x = -3, 0, 7`, the setup multiplication and addition counts when composing the three pairs, and the counts over those three inputs with and without preparation. Count calls to the source-level arithmetic helpers, not CPU instructions:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/affine_schedule.cpp -o "${TMPDIR:-/tmp}/affine_schedule" && "${TMPDIR:-/tmp}/affine_schedule"
   ```
   Answer format: the composition rule, the prepared pair, the three outputs, then setup counts and the per-route counts.
<details><summary>Answer</summary>
Key:

  - Applying `(a,b)` and then `(c,d)` gives `(c*a, c*b+d)`. Keep the order: the second transformation acts on the first one's result.
  - The prepared pair is `(-24,1)`. The outputs are `73, 1, -167`, with zero mismatches.
  - Composing two pairs takes two multiplications and one addition, so the two compositions cost four multiplications and two additions during setup.
  - Across three inputs, the direct route records nine multiplications and nine additions; the prepared route records three of each, setup excluded. The work left after each input arrives is one multiplication and one addition.
  - The counts describe the instrumented algorithm. They show neither an instruction count nor a speedup. Benchmark the compiled code before claiming a latency improvement.
  - Holds for / re-check: Apple clang 21 (macOS arm64) and GCC 13.3 (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`. The small signed values and all intermediates fit `int64_t`; a result outside the type's range is undefined ([expr.pre] p4). Preparation holds while the coefficients and their order stay fixed. Rebuild the pair when either changes; re-check range and numerical equivalence before using larger or floating-point inputs.
</details>

---

## Close

Next: part 12, `lesson-06-practice-bank-12-os-reports.md` (about 28 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
