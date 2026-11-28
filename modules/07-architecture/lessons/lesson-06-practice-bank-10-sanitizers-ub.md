# Lesson 06 — Practice Bank, part 10 of 11 (optional): Undefined Behaviour and Sanitizers

**Time: about 29 minutes (estimate)** for 4 cards (8 + 3 + 12 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: conversions that are undefined, pattern initialization, address reuse, and signed overflow.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB25 · Spot the trap
Q: Which of these is undefined behaviour in C++20, and which is not? (A) `static_cast<std::int32_t>(3.0e9f)`. (B) `static_cast<std::int64_t>(UINT64_MAX)`. How would you catch (A) in a test run? `float_cast.cpp` does (A) when given the argument `big`:
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=float-cast-overflow modules/07-architecture/lab/practice/float_cast.cpp -o "${TMPDIR:-/tmp}/float_cast" && "${TMPDIR:-/tmp}/float_cast" big
   g++ -std=c++20 -O1 -g -fsanitize=undefined modules/07-architecture/lab/practice/float_cast.cpp -o "${TMPDIR:-/tmp}/float_cast" && "${TMPDIR:-/tmp}/float_cast" big
   ```
   Answer format: (A) and (B) with the rule each, then the sanitizer setting per compiler.
<details><summary>Answer</summary>
Key:

  - (A) is undefined: a floating value whose truncated value does not fit the integer type ([conv.fpint] p1). UBSan with `-fsanitize=float-cast-overflow` reports "3e+09 is outside the range of representable values of type 'int'"; the program then printed 2147483647, which you must not rely on.
  - (B) is defined since C++20: integer-to-integer conversion gives the value congruent modulo 2^N, here -1 ([conv.integral] p3). Before C++20 it was implementation-defined, still not undefined.
  - Rule: float-to-int is the dangerous direction. Range-check before the cast, and keep `-fsanitize=float-cast-overflow` in your test builds. It is part of `-fsanitize=undefined` on Clang, but not on GCC 13.3: there `-fsanitize=undefined` printed nothing for (A) and you must add `-fsanitize=float-cast-overflow` explicitly.
  - Holds for / re-check: C++20 and later for (B); C++17 wording (N4659 [conv.integral] p3) checked for the older rule. UBSan run with Apple clang 21 (arm64), and on clang 18.1.3 and GCC 13.3 (Linux aarch64). Which checks `-fsanitize=undefined` enables differs per compiler; re-run the probe on yours.
</details>

### Card M7-PB26 · Recall
Q: Your crash dump shows a local `int` holding `0xaaaaaaaa` on one build and `0xfefefefe` on another; both builds use `-ftrivial-auto-var-init=pattern` (the compiler writes a pattern into every automatic variable that has no initializer). What explains it, and why should a test never assert on the pattern value? Run with no arguments, on each compiler you have:
   ```sh
   clang++ -std=c++20 -O0 -ftrivial-auto-var-init=pattern modules/07-architecture/lab/practice/autoinit_pick.cpp -o "${TMPDIR:-/tmp}/autoinit_pick" && "${TMPDIR:-/tmp}/autoinit_pick"
   ```
   Answer format: the explanation with both values, the documentation point, then the rule.
<details><summary>Answer</summary>
Key:

  - The pattern is compiler-specific: Apple clang 21 `=pattern` prints `stack value 0xaaaaaaaa`; GCC 13.3 `=pattern` prints `0xfefefefe`.
  - GCC's docs say the current byte is 0xFE and that the pattern may change in the future, so it is not a contract.
  - Use the pattern to make bugs visible (it is an unlikely value), not as a value to test for.
  - Holds for / re-check: Apple clang 21.0.0 arm64, GCC 13.3 aarch64, `-O0`. Re-check with the same one-line run on a new compiler; read the compiler's docs for `pattern` (gcc.gnu.org, Optimize-Options).
</details>

### Card M7-PB28 · Spot the trap
Q: A bug only shows when a freed address is handed straight back to the next same-size allocation (for example, code that caches a raw pointer value after the object died). Your normal test fails sometimes; the AddressSanitizer (ASan) build passes every time. A teammate concludes "ASan is clean, so there is no such bug". Measure why. `reuse_probe.cpp` allocates through `reuse_shim.cpp` (it wraps `malloc`/`free` in a separate file so the optimizer cannot delete the allocations), records each address as an integer before freeing it, then allocates again and counts how often the address matches, over 257 rounds. The script builds and runs (1) a plain `-O2` build, (2) an ASan build, (3) the ASan build with `ASAN_OPTIONS=quarantine_size_mb=0`, 3 runs each, and first checks in the listing that the probe still contains the allocation calls. (ASan's *quarantine* holds freed blocks for a while before they can be handed out again, so use-after-free reads can be caught.)
   ```sh
   bash modules/07-architecture/lab/practice/reuse_check.sh clang++
   ```
   (On Linux, also `g++`. The single-file control `reuse_inline.cpp` gives its own command on line 3.)
   Answer format: reuse counts per build, what that means for the teammate, then the listing check and the habit.
<details><summary>Answer</summary>
Key:

  - Reuse counts (3 of 3 runs): glibc plain build 257/257 on both GCC 13.3 and clang 18; macOS malloc plain build varies from run to run (57 to 257 of 257 seen in 15 runs); not graded; ASan default 0/257 on all three; ASan with `quarantine_size_mb=0` 257/257 on all three.
  - Reading: ASan's quarantine removes the very condition the bug needs. A pass under ASan does not show the bug is absent; it can mean the trigger never happened. Fix the method: first check that the sanitizer build can still produce the condition, then use sanitizers for what they detect (bad reads and writes), not as proof about reuse-dependent logic.
  - The probe's call sites are verified: 4 calls to the shim remain in the listing in both plain and ASan builds on all three toolchains. Control for why: the single-file probe `reuse_inline.cpp` (`new`/`delete` inline) had zero allocation calls at `-O2` on Apple clang and clang 18 (printing 0 reuse by construction), and kept all 4 on GCC. A zero count means nothing until the listing shows the allocations survived.
  - Habit: capture the address as an integer before `free`; do not use the freed pointer value in comparisons (a freed pointer's value is an invalid pointer value, whose non-dereferencing uses are implementation-defined, [basic.stc]/4).
  - Holds for / re-check: Apple clang 21 ASan on macOS, GCC 13.3 and clang 18.1.3 on Ubuntu 24.04 aarch64. Quarantine size and defaults differ between versions and platforms. Re-check with the probe on your toolchain; the macOS plain count varies from run to run and is not graded.
</details>

### Card M7-PB29 · Spot the trap
Q: A benchmark hashes symbols with `int h = 0; for (...) h = h * 31 + c;` and prints the hash so the work is not deleted. It compiles cleanly with `-Wall -Wextra`, and for short symbols it matches a `uint32_t` version. What is wrong? Build `rolling_hash.cpp` with UBSan (`-fsanitize=undefined`, which checks some kinds of undefined behaviour while the program runs) and run it with exactly the inputs `abc` and `abcdefghijkl`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -fsanitize=undefined modules/07-architecture/lab/practice/rolling_hash.cpp -o "${TMPDIR:-/tmp}/rolling_hash" && "${TMPDIR:-/tmp}/rolling_hash" abc && "${TMPDIR:-/tmp}/rolling_hash" abcdefghijkl
   ```
   Answer format: warnings, the result for each input, then the rule and the fix.
<details><summary>Answer</summary>
Key:

  - No compile-time warning on any of the three compilers.
  - 3 characters: no report, both hashes equal (96354).
  - `abcdefghijkl`: the first report is `signed integer overflow: 92599395 * 31 cannot be represented in type 'int'` at the multiply (Apple clang 21, GCC 13.3, clang 18.1.3; 3 runs each). With these recovering builds the program goes on and prints signed and unsigned 306089158. Treat that value as incidental: after UB the program has no guaranteed result. Matching bits hide the bug in the output.
  - Rule: a result outside the range of the type is undefined behaviour for signed integers ([expr.pre] p4); unsigned arithmetic is defined modulo 2^N ([basic.fundamental] p2). Once UB is possible the optimizer may assume it never happens, so the timed code may not be the code you wrote.
  - Fix: for a hash that is meant to wrap, use `uint32_t` or `uint64_t` (defined modulo 2^N). A counter that must detect overflow is a different problem: check its range before the add, do not rely on wrapping. Run benchmarks once under UBSan with realistic input, because the overflow appears only for some inputs.
  - Holds for / re-check: the rule is in the language (C++20 draft; C++20 still leaves signed overflow undefined). Sanitizer wording checked on Apple clang 21, GCC 13.3 and clang 18.1.3. Re-check: rerun `rolling_hash.cpp` under `-fsanitize=undefined` on your compiler.
</details>

---

## Close

Next: part 11, `lesson-06-practice-bank-11-concurrency-env.md` (about 13 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
