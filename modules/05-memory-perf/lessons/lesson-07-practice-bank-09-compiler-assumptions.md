# Lesson 07 — Practice Bank, part 9 of 9 (optional): What the Compiler Assumes

**Time: about 26 minutes (estimate)** for 3 cards (6 + 8 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: `[[assume]]`, automatic zero-initialization, and opaque calls.

Hook: What does an optimizer do with a promise about a value that turns out to be false?

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

### Card M5-PB1 · Spot the trap
Q: A function contains `[[assume(i >= 0 && i < 8)]];` and then reads `table[i & 7]` from an 8-entry table. Someone calls it with 9 and says "the mask keeps it in range, and it printed a value, so it is fine". Build it with no sanitizer at -O0, -O1 and -O2, and run each binary with 3 and with 9. Predict first: will all three builds print the same thing for 9?
   ```sh
   for o in 0 1 2; do clang++ -std=c++23 -O$o modules/05-memory-perf/lab/practice/assume_mask.cpp -o "${TMPDIR:-/tmp}/assume$o" && "${TMPDIR:-/tmp}/assume$o" 3 && "${TMPDIR:-/tmp}/assume$o" 9; done
   ```
   Answer format: what each build prints for 9, then why, in one line.
<details><summary>Answer</summary>
Key:

  - With 3 every build prints `lookup(3)=4`.
  - With 9 the builds disagree: `lookup(9)=2` at -O0 (9 & 7 = 1, so `table[1]`), and `lookup(9)=0` at -O1 and -O2 (same in 3 runs of each).
  - Why: a false assumption is undefined behaviour ([dcl.attr.assume] p1 in C++23, N4950; the 2026 drafts word it as runtime-undefined behaviour). It is a precondition, not a comment. At -O2 the compiler used it: the `and` with 7 is gone from the assembly, so the load reads past the end of the table. The "safe" mask was removed because you promised it was not needed.
  - After undefined behaviour, the printed value is not a fact about your code. Different -O levels giving different answers is a strong hint that you have UB.
  - Fix: guarantee the precondition at the caller, or check it and handle the error, before assuming it.
  - Holds for / re-check: the values printed after the UB are specific to Apple clang 21 on arm64 and can change with any compiler, flag or layout change; the rule that a false assumption is UB is in the C++23 and C++26 drafts. A compiler that does not know the attribute ignores it: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), clang 18 warned `unknown attribute 'assume' ignored`, kept the mask and printed 2 at every level; GCC 13 printed 0 at -O1 and -O2. Re-check by building at several -O levels and reading the assembly of the function.
</details>

### Card M5-PB22 · Apply
Q: A team builds with `-ftrivial-auto-var-init=zero` (the compiler writes zeros into every automatic variable that has no initializer). A 1 KiB local histogram filled by a call into another file is in a hot function, so they opt that one variable out. Developer A writes `uint32_t hist[256] [[indeterminate]];`, developer B writes `uint32_t hist[256] __attribute__((uninitialized));`. Both compile. Which one actually removes the fill on Apple clang 21 and on GCC 13, and how do you check without timing anything?
   ```sh
   clang++ -std=c++2c -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -
   g++ -std=c++23 -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -
   ```
   Answer format: the check, one line per compiler, then the lesson.
<details><summary>Answer</summary>
Key:

  - Check: compile with `-O2 -S` and count calls to `memset`/`bzero` in each function. Careful: GCC's identical-code folding (ICF) merges `std_opted_out` into a tail jump to `plain`, so a per-function count shows 0 for the wrong reason. Run it twice, with and without `-fno-ipa-icf`, or look for the `b _Z5plainv` jump.
  - Apple clang 21 (`-std=c++2c`): `[[indeterminate]]` gives `warning: unknown attribute 'indeterminate' ignored`; the fill call is still there (1 call). `__attribute__((uninitialized))`: 0 calls.
  - GCC 13.3 (`-std=c++23`): `warning: 'indeterminate' attribute directive ignored`. Default build: `std_opted_out` counts 0 `memset` calls, but only because it was folded into a jump to `plain` (1 call). With `-fno-ipa-icf`: `std_opted_out` = 1 call. `__attribute__((uninitialized))`: 0 calls.
  - Lesson: an unknown standard attribute is ignored with only a warning. Turn on `-Werror=unknown-attributes` / `-Werror=attributes`, and confirm in the asm.
  - Holds for / re-check: Apple clang 21.0.0 arm64 and GCC 13.3 aarch64, `-O2` (GCC 13.3 does not know `-std=c++2c`; use `-std=c++23`). Newer compilers that implement C++26 will accept `[[indeterminate]]`; re-check with the same count after any compiler upgrade.
</details>

### Card M5-PB24 · Apply
Q: `escape_opaque.cpp` declares `void observe(const int*);` with no body (an *opaque* call: the compiler cannot see what it does) and has four functions that sum an `int` array into a local `acc`: `sum_local` (no call), `sum_escape_each` (calls `observe(&acc)` inside the loop), `sum_escape_before` (calls `observe(&acc)` once, before the loop), `sum_snapshot` (copies `acc` into `int snap` and calls `observe(&snap)` inside the loop). Predict, for each, whether `acc` is loaded from and stored to the stack on every loop step. Then check the loop bodies (and with GCC, `g++` on Linux, the same way):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/escape_opaque.cpp -o -
   ```
   Answer format: yes or no for each of the four functions, then the rule.
<details><summary>Answer</summary>
Key:

  - `sum_escape_each`: yes. Each step stores `acc` to its stack slot before the call and reads it back after (Apple clang `str`/`ldr [sp,#12]`; GCC `str`/`ldr [sp,4]`; x86 clang 19.1 does `addl %eax, 4(%rsp)`). The callee might have kept the pointer and changed `acc` (the `const` in the parameter does not stop that, because `acc` itself is not const).
  - `sum_snapshot`: no. `acc` stays in a register; only `snap` is stored, and never read back. Escaping a copy protects the original.
  - `sum_escape_before`: no. One reload after the single call, then the loop runs on registers (Apple clang and GCC -O3 even vectorize it). With no opaque call inside the loop, nothing can observe `acc` there.
  - `sum_local`: no memory traffic for `acc` at all.
  - Rule to keep: what blocks optimization is a call to unseen code while the variable's address is out, not the variable's type or the syntax around it.
  - Holds for / re-check: Apple clang 21 arm64, GCC 13.3 aarch64 (-O2 and -O3), gcc 14.3 and clang 19.1 x86-64 (Compiler Explorer) at -O2. Whether the loop also vectorizes depends on compiler and -O level (GCC 13.3 -O2 did not vectorize `sum_escape_before`; -O3 did). Re-check by reading the loop body after any compiler upgrade.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
