# Lesson 07 — Practice Bank, part 6 of 9 (optional): Loops the Compiler Vectorizes

**Time: about 22 minutes (estimate)** for 2 cards (12 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: a clamp in four forms, and a loop with an early exit.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

### Card M5-PB17 · Apply
Q: `clamp.cpp` sets negative values to zero in four ways: element type `int` or `signed char`, and the store written as `if (a[i] < 0) a[i] = 0;` or as `a[i] = a[i] < 0 ? 0 : a[i];`. Before you run anything, predict for AArch64: on plain NEON (the default target), which of the four loops end up with whole-vector stores? Then predict what changes when the target also has SVE (predicated vector instructions). Run the script and read the count lines. Why can the `if` form not simply use a plain vector store?
   ```sh
   bash modules/05-memory-perf/lab/practice/clamp_sve.sh clang++
   clang++ -std=c++20 -O2 -g -fsanitize=thread modules/05-memory-perf/lab/practice/clamp_reader.cpp modules/05-memory-perf/lab/practice/clamp.cpp -o "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" always
   ```
   (The second command runs one clamping thread against one reader thread, first with the `if` form, then with the always-store form.)
   Answer format: why the `if` form is special, the NEON counts, the SVE counts, then what the always-store rewrite requires.
<details><summary>Answer</summary>
Key:

  - The `if` form writes only some elements. The compiler may not turn that into "write every element back": a store the program never asked for could conflict with another thread's read of an element the original code only reads ([intro.races] Note 22, C++23 draft N4950). So the `if` form needs a store that writes only the selected elements, or it stays one test and one store per element.
  - Plain NEON, Apple clang 21: `clamp32_if` and `clamp8_if` have 0 vector stores (20 and 48 conditional branches). Both `always` forms have 1 plain vector store.
  - With `-march=armv8-a+sve`, Apple clang 21: `clamp32_if` has 2 predicated stores (`st1w`) and `clamp8_if` has 3 (`st1b`), and no plain vector store. clang 18.1.3 gives the same 2 and 3. GCC 13.3 with SVE has 1 predicated store in each of the four loops.
  - So the same source goes from per-element stores to predicated vector stores when the target gains a predicated store. The source did not change; the target did.
  - Writing the store unconditionally (the `always` form) adds stores. They require that no unsynchronized access in another thread conflicts with them; concurrent readers can make this rewrite invalid. (Under ThreadSanitizer with one reader thread over 1,024 positive ints: the `if` form printed a checksum and no report; the `always` form reported a data race, a read of size 8 against a previous write by the clamping thread.) A concurrent non-atomic writer already conflicts with the original loop's reads.
  - Holds for / re-check: the NEON `if` loops contain vector work but scalar stores; do not call the whole loop scalar. Counts are from cross-compiled listings, `-O2` for clang and `-O2`/`-O3` for GCC. No SVE code was run (the M1 Max has none). Counts of the `always` forms under SVE differ between compilers and are not part of the answer. In these listings NEON has no masked store; the architecture manuals were not opened. GCC 13.3 on aarch64 without SVE (`-O3`) leaves both `if` forms scalar with `not vectorized: control flow in loop`. A newer compiler or a different element type can change every line; re-check by running the script on your compiler and target.
</details>

### Card M5-PB18 · Spot the trap
Q: `calib_stop()` adds an offset to sensor readings and returns at the first fault marker. `calib_all()` converts every reading, ORs a "fault seen" flag, and finds the fault position after the loop. Both return the same index. Assume the input and output ranges are disjoint, all n input elements are readable, and every input-plus-offset value is representable in `int32_t`. The input has 1000 readings with a fault at index 400, and both output buffers start filled with -1. Predict: in how many output elements do the two functions differ? Is the second a drop-in replacement?
   ```sh
   bash modules/05-memory-perf/lab/practice/calib_report.sh clang++
   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/calib.cpp modules/05-memory-perf/lab/practice/calib_alias.cpp -o "${TMPDIR:-/tmp}/calib_alias" && "${TMPDIR:-/tmp}/calib_alias"
   ```
   (The second command calls both functions in place, `in == out`.)
   Answer format: the count, why a test can miss it, when the rewrite is valid, then what you get in exchange.
<details><summary>Answer</summary>
Key:

  - They differ in 600 elements: indices 400 to 999. The convert-all version writes past the fault. The stopping version leaves those elements untouched.
  - With no fault the buffers are identical (0 differences). So a test that uses only clean input, or compares only return values, passes and hides the change.
  - The rewrite is valid only if the contract allows it: the caller must not rely on the tail being untouched, and nobody else may write those elements meanwhile. It also needs the input unchanged until its second search, so it is not an in-place replacement: called with `in == out` on input `{0, INT32_MIN, 2}` and offset 7, the stopping version returns 1, but the convert-all version overwrites the marker, its unbounded search runs off the array, and ASan reports a stack-buffer-overflow READ of size 4 in `calib_all`. Aliasing is another correct answer to the drop-in question.
  - What you get in exchange: all three compilers checked report the stopping loop as not vectorized and the convert-all loop as vectorized.
  - Habit: before comparing two variants for speed, compare their full outputs on inputs that trigger the rare case.
  - Holds for / re-check: element counts hold on any conforming compiler (they follow from the source). The vectorization reports are from Apple clang 21 (`-O2`), clang 18.1.3 (`-O2`) and GCC 13.3 (`-O3`, aarch64). The reason text differs by version: clang 18 says `could not determine number of loop iterations`, Apple clang 21 says `Cannot vectorize early exit loop with writes to memory`, GCC 13 says `control flow in loop`. The 600 count assumes the disjoint buffers stated in the question. A later compiler may vectorize the stopping loop; re-run the script to see.
</details>

---

## Close

Next: part 7, `lesson-07-practice-bank-07-cheaper-arithmetic.md` (about 20 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
