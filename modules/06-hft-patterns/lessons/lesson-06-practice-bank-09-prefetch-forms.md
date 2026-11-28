# Lesson 06 — Practice Bank, part 9 of 12 (optional): Prefetch Forms

**Time: about 23 minutes (estimate)** for 3 cards (5 + 10 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what each `__builtin_prefetch` form becomes on arm64 and x86-64, what a run-time locality does, and which x86-64 build flags keep a write hint.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB20 · Apply
Q: A teammate wants one helper, `prefetch(p, k)`, whose locality `k` (0-3) is chosen at run time from a config value, and writes `__builtin_prefetch(p, 0, k)` as in `prefetch_var.cpp`. Predict what each compiler does with it, then check. Then write a version that keeps the run-time choice and compiles, and check which instructions it emits (card M6-PB21 has the full mapping).
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/prefetch_var.cpp -o -
   ```
   (On Linux, also try `g++` with the same flags.)
   Answer format: what each compiler does, your fix in one or two lines, then what it emits.
<details><summary>Answer</summary>
Key:

  - A non-constant locality is a compile error, not a silent default: Clang says "argument to '__builtin_prefetch' must be a constant integer"; GCC 13.3 says "third argument to '__builtin_prefetch' must be a constant". The rw argument has the same rule.
  - Fix: keep each call's arguments constant and choose among them at run time, for example `template <int L> void pf(const void* p) { __builtin_prefetch(p, 0, L); }` plus a `switch (k)` that calls `pf<0>` ... `pf<3>`. A run-time branch now picks one of four constant hints.
  - Check the listing: on arm64 the four cases emit four different `prfm` forms, so each locality survived as its own instruction (card M6-PB21 asks which form each one is).
  - Holds for / re-check: Apple clang 21 (arm64), and GCC 13.3 / clang 18.1.3 on aarch64 Linux; the constant-argument rule is in GCC's `__builtin_prefetch` documentation (Other-Builtins). Whether a run-time-chosen hint is worth its branch was not measured. Re-check with `-S` on your compiler.
</details>

### Card M6-PB21 · Apply
Q: Predict the instruction each `__builtin_prefetch` form becomes on arm64 and on baseline x86-64: `(p,0,0)`, `(p,0,1)`, `(p,0,2)`, `(p,0,3)` and `(p,1,3)`. Then build `prefetch_forms.cpp` for both targets, and once more for x86-64 with `-mprfchw` added. Which hint is silently weakened on baseline x86-64, and how many warnings do you get?
   ```sh
   clang++ --target=arm64-apple-macos -std=c++20 -O2 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -
   ```
   Answer format: the five arm64 instructions in order, the five x86-64 ones, then the weakened hint and the habit.
<details><summary>Answer</summary>
Key:

  - arm64: `(p,0,0)` -> `prfm pldl1strm`; `(p,0,1)` -> `prfm pldl3keep`; `(p,0,2)` -> `prfm pldl2keep`; `(p,0,3)` -> `prfm pldl1keep`; `(p,1,3)` -> `prfm pstl1keep` (the "st" form is the write hint).
  - x86-64 baseline: `prefetchnta`, `prefetcht2`, `prefetcht1`, `prefetcht0`, and the write hint `(p,1,3)` also becomes plain `prefetcht0`. With `-mprfchw` it becomes `prefetchw`.
  - GCC 13.3 x86-64 (Compiler Explorer, `-O2`): same mapping, write hint `prefetcht0`, and `prefetchw` with `-mprfchw`.
  - So the write intent is dropped without a warning (0 warnings with `-Wall -Wextra`) unless the target has the PRFCHW feature enabled: the compiler still emits a read prefetch, it does not drop the instruction. (GCC's documented "no other code is generated" rule is for targets with no prefetch at all; it is not what happened here.)
  - Habit: read the assembly for the exact `-march` you ship before you trust a hint.
  - Holds for / re-check: Apple clang 21.0.0 (arm64-apple-macos, and x86-64 compile only) and GCC 13.3 x86-64 on Compiler Explorer; GCC's `__builtin_prefetch` documentation (gcc.gnu.org, Other-Builtins) gives locality 0-3, default 3. Other compilers and versions may map locality differently; re-check with `-S`. Whether the hint helps speed was not measured.
</details>

### Card M6-PB29 · Spot the trap
Q: Your release build uses `-march=x86-64-v3` ("we build for modern CPUs, AVX2 and all"), and a hot loop calls `__builtin_prefetch(p, 1, 3)` and `(p, 1, 0)` to ask for lines it will write. Does the write intent survive that build? Card M6-PB21 covered plain `-O2` and `-mprfchw`; decide which of these x86-64 flag sets keep it: `-O2 -march=x86-64-v3`, `-O2 -march=skylake`, `-O2 -march=znver3`. Then check with the one clang on your Mac (compile only; `prefetch_six.cpp` has the two write forms last), and say how a build would catch a regression.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra --target=x86_64-linux-gnu -S modules/06-hft-patterns/lab/practice/prefetch_six.cpp -o - | grep -E 'prefetch'
   ```
   (Repeat with each flag set added after `-O2`.)
   Answer format: which flag sets give `prefetchw`, why v3 does not, then the check you would add.
<details><summary>Answer</summary>
Key:

  - `prefetchw` for both write forms with `-march=skylake` or `-march=znver3` (as with `-mprfchw`). With `-march=x86-64-v3`, as with plain `-O2`, the write prefetches silently become read prefetches (`prefetcht0` and `prefetchnta`), with no warning.
  - Why: `PREFETCHW` has its own CPUID feature flag (PRFCHW), and the x86-64-v3 level does not include it, although it includes AVX2. The compiler emits it only when the target says the CPU has it. So "built for modern CPUs" does not imply write prefetch.
  - Catch it: a build check that greps the release listing (or the disassembly) for `prefetchw` in the hot function and fails when it is missing; nothing else reports the downgrade.
  - Write intent does not guarantee that the line is fetched with, or keeps, exclusive ownership, and the instruction says nothing about speed: that needs a measurement on the target CPU (the Intel manual says the hints are implementation-dependent and may be ignored).
  - Holds for / re-check: Apple clang 21 targeting x86_64-linux-gnu (compile only, nothing x86 was run); gcc 13.3 and clang 18.1 on Compiler Explorer gave the same flag-set split. The mapping can change with compiler version or `-march` level definitions; re-check with your real build flags.
</details>

---

## Close

Next: part 10, `lesson-06-practice-bank-10-prefetch-manuals.md` (about 24 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
