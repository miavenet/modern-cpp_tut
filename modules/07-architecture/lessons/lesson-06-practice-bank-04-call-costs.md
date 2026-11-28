# Lesson 06 — Practice Bank, part 4 of 11 (optional): What a Call Costs

**Time: about 24 minutes (estimate)** for 3 cards (8 + 10 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: a rival library in another file, an opaque call, and copies in recursive lambdas.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB17 · Spot the trap
Q: You compare your inlined data structure against a rival library that you call through a small accessor compiled in its own file (its own translation unit). `boundary/lib.cpp` holds `extern "C" uint64_t book_spread(const Book*)`; `boundary/app.cpp` calls it 1000 times in a loop. Build twice, without and with `-flto` (link-time optimization) on all three lines, and count calls to the accessor in `main`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/07-architecture/lab/practice/boundary/lib.cpp -o "${TMPDIR:-/tmp}/lib.o"
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/07-architecture/lab/practice/boundary/app.cpp -o "${TMPDIR:-/tmp}/app.o"
   clang++ -O2 "${TMPDIR:-/tmp}/lib.o" "${TMPDIR:-/tmp}/app.o" -o "${TMPDIR:-/tmp}/app_bin" && "${TMPDIR:-/tmp}/app_bin"
   objdump -d --no-show-raw-insn "${TMPDIR:-/tmp}/app_bin" | awk '/<_?main>:/{p=1;next} /^$/{p=0} p' | grep -cE '(bl|call).*_?book_spread'
   ```
   What does the count tell you about the fairness of the comparison?
   Answer format: the count without and with LTO, then the fix.
<details><summary>Answer</summary>
Key:

  - Without LTO: 1 call instruction to `book_spread` in `main` (Apple clang 21, and GCC 13.3 and clang 18 on Linux all gave 1), executed every iteration. The compiler cannot see the body across the translation-unit boundary, so it cannot inline it or optimize around it.
  - With `-flto`: 0 calls (Apple clang 21 and GCC 13.3; the clang 18 Linux LTO build also gave 0). The link-time optimizer inlined it. Every build prints the same result (1000).
  - So the rival paid for a real function call in every iteration and your structure did not. That is a property of the harness, not of the rival.
  - **Fix:** build every contender the same way (same language, same translation unit or same LTO setting). Check by counting calls in the listing, as above.
  - Holds for / re-check: Apple clang 21 (Mach-O, symbols have a leading underscore: `_main`, `_book_spread`) and GCC 13.3 / clang 18 on aarch64 Linux (ELF, no underscore), which is why the pattern uses `_?`. `bl` is the arm64 call instruction; on x86 the call is `call`, which the pattern also matches (not run on x86 here). If the awk range finds no `main`, the count is 0 for the wrong reason: print the range first. LTO behaviour varies with compiler and linker; re-check with the count command after any toolchain change.
</details>

### Card M7-PB27 · Apply
Q: A teammate writes in a review: "`observe()` is in another `.cpp` file, so the loop in `sum_escape_each` will always keep `acc` in memory; that is a fixed property of the source." `sum_escape_each` (in `lto_sum.cpp`) adds each element to a local `acc` and calls `observe(&acc)` every step; `observe` is defined in `lto_observe.cpp` as an empty function. Card M7-PB17 showed that `-flto` can inline across files. Predict, for `sum_escape_each` without and with `-flto`: the calls to `observe`, the stack loads and stores of `acc` in the loop, and whether the loop vectorizes. How do you show it from the final binary (not from `-S`)?
   ```sh
   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/lto_sum.cpp modules/07-architecture/lab/practice/lto_observe.cpp -o "${TMPDIR:-/tmp}/lto_off" && objdump -d "${TMPDIR:-/tmp}/lto_off"
   clang++ -std=c++20 -O2 -flto modules/07-architecture/lab/practice/lto_sum.cpp modules/07-architecture/lab/practice/lto_observe.cpp -o "${TMPDIR:-/tmp}/lto_on" && objdump -d "${TMPDIR:-/tmp}/lto_on"
   ```
   Answer format: calls, stack accesses and vectorization without and with `-flto`, how you show it, then the rule.
<details><summary>Answer</summary>
Key:

  - Link-time optimization (`-flto`) breaks the claim. With it the "other file" is visible at link time, the empty `observe` is inlined, and the loop needs no stack slot and no call.
  - Show it: build both files with and without `-flto`, then `objdump -d` the linked binary and read `sum_escape_each`.
  - Observed: without LTO, 1 call to `observe` and stack loads/stores in the loop (Apple clang 21 and GCC 13.3). With LTO, 0 calls, 0 stack accesses, and a vector loop (`add.4s` / `add v0.4s`) on both; GCC even made a constant-propagated copy (`.constprop.0`).
  - So "opaque" is a property of the build (separate compilation, LTO, inlining limits), not of the source. A benchmark or a barrier that relies on an opaque call must state the build flags and be re-checked when they change.
  - Holds for / re-check: Apple clang 21 + ld64 (arm64) and GCC 13.3 + GNU ld (aarch64), `-O2`. LTO inlining decisions depend on size heuristics; a large `observe` might not be inlined. Re-check with `objdump -d` of the final binary.
</details>

### Card M7-PB32 · Apply
Q: `recursive_self.cpp` captures a `Cargo` by value in each of two recursive lambdas. `Cargo`'s copy constructor increments a counter. Each call adds `cargo.unit` (2), recursing from depth 3 to 0. One lambda takes `this auto self`, the other `this const auto& self` (explicit object parameters, lesson 03 CONCEPT C7). Predict both sums and both copy counts, including the initial call from an lvalue closure, then run:
   ```sh
   clang++ -std=c++23 -O2 -Wall -Wextra modules/07-architecture/lab/practice/recursive_self.cpp -o "${TMPDIR:-/tmp}/recursive_self" && "${TMPDIR:-/tmp}/recursive_self"
   ```
   Answer format: sum and copies for each lambda, then one line on where the copies come from.
<details><summary>Answer</summary>
Key:

  - Both return 8. The value-self lambda records 4 copies; the reference-self lambda records 0.
  - The four value-self invocations are depths 3, 2, 1, 0. The initial lvalue call copies too; counting only recursive edges would wrongly predict 3.
  - For this read-only captured state, reference self keeps the result and avoids the observed closure copies. This is a copy-count result, not a timing claim.
  - Holds for / re-check: Apple clang 21, C++23, -O2; the counter is reset after each closure is built. Moving the initial closure, changing its capture types or changing the self parameter changes what must be checked; rerun the probe.
</details>

---

## Close

Next: part 5, `lesson-06-practice-bank-05-build-output.md` (about 27 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
