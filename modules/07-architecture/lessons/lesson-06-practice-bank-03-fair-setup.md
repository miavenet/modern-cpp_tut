# Lesson 06 — Practice Bank, part 3 of 11 (optional): A Fair Setup

**Time: about 30 minutes (estimate)** for 3 cards (12 + 12 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: pre-touched memory, inputs that repeat, and background load that really loads.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB5 · Apply
Q: You compare two order-book node allocators. Allocator A grabs one large block at startup and touches it during setup. Allocator B asks the OS for memory as the book grows. A wins clearly. Before believing that the allocation algorithm is better, list three differences that are not the algorithm, and one measurement for each that would expose it. Then run this program to see the second one:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/cache_warm.cpp -o "${TMPDIR:-/tmp}/cache_warm" && "${TMPDIR:-/tmp}/cache_warm"
   ```
   Answer format: three differences, each with its measurement.
<details><summary>Answer</summary>
Key:

  - First-touch page faults land inside B's timed region but not A's. Count faults per timed run (`getrusage` `ru_minflt`); compare B with its memory pre-touched.
  - A's memory is cache-warm: setup just walked it, so its first timed accesses hit cache. Fix by evicting (stream a large unrelated buffer) or by timing both from the same cache state. In the program both buffers are pre-faulted (0 faults), yet in 15 reps over 3 process runs on a quiet machine every random pointer chase over 1 MiB was slower after streaming 256 MiB through than right after setup. The microseconds rise under load (one machine), so judge by the pattern, not the figures.
  - Different work: check both produce the same book state (a checksum) before comparing times.
  - Accept other non-algorithm differences if each comes with a measurement that exposes it.
  - Holds for / re-check: the method holds on any OS. The cache numbers are from one machine and depend on cache sizes. The test uses a dependent random chase so that hardware prefetching cannot hide misses.
</details>

### Card M7-PB9 · Apply
Q: You benchmark a filter that keeps the index of every price above a limit: `if (px[i] > limit) out[m++] = i;`. To save time you generate a short block of prices and repeat it to fill 16M elements. The branchy loop looks almost free. What could make that result too optimistic for live data, and what one change to the input tests it? Build it and run both inputs (arguments: target pass rate, repeat period; 0 = never repeats):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/filter.cpp -o "${TMPDIR:-/tmp}/filter" && "${TMPDIR:-/tmp}/filter" 0.8 1000 && "${TMPDIR:-/tmp}/filter" 0.8 0
   ```
   Answer format: the cause in one line, the input change in one line, then what you measured.
<details><summary>Answer</summary>
Key:

  - A branch predictor can learn a repeating sequence of outcomes. Then a repeated block mispredicts far less than fresh data with about the same pass rate, so the benchmark hides the misprediction cost.
  - Test: keep the pass rate, remove the repetition (period 0), and compare. Also print the realized pass rate of each input, since "the same rate" is only approximate: here 0.794 (period 1000) vs 0.800 (no repeat).
  - First confirm the loop really has a branch: in the `-O2` listing (`-S`), `branchy` has a `cmp` + `b.ls` per element. A branch-free control (`out[m] = i; m += px[i] > limit;`) should not care about the input.
  - Measured here, in every run: branchy was faster per element with period 1000 than with no repetition, and the branch-free control ran at about the same speed on both inputs. The ns/elem figures and the ratio change with machine and load and are not part of the Key; grade the direction and the control, and a different ratio, or an inconclusive comparison you report as such, does not lower the grade.
  - The predictor here also learned a 10,000-element period (as fast as period 1000 in every run). A 100,000-element period was slower than period 1000 and faster than no repetition in every run here: partly learned. So "long enough to be safe" must be measured too, not guessed.
  - Rule: generate benchmark inputs without repetition, or replay recorded production data.
  - Holds for / re-check: how long a pattern the predictor learns depends on the core, so the periods above are this M1 Max's. Another compiler version or flag set may turn the branch into branch-free code; check the listing each time. Re-check on each new CPU by sweeping the period (for example 1000, 10^4, 10^5, 0). On Linux, `perf stat -e branch-misses` shows the mechanism directly.
</details>

### Card M7-PB18 · Spot the trap
Q: You want a background "noise" function that keeps the memory system busy, so you can measure another routine under load. `noise.cpp` has two versions: `noise_plain(n)` allocates a `std::vector<std::uint32_t>` of n elements and fills it with a hash of the index, never reading it back; `noise_volatile(n)` does the same fill through a `volatile std::uint32_t*`. Without running anything, compile to assembly and, for each function, count the calls to `operator new` (`_Znwm`) and say whether it contains a store loop:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/07-architecture/lab/practice/noise.cpp -o -
   ```
   Does the noise function you wrote actually run?
   Answer format: one line per function per compiler, then the habit.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 (arm64) and clang 18 (Linux aarch64): `noise_plain` contains 0 calls to `operator new` and no store loop; what is left is a length check that jumps to the `throw` path (6 instructions on Apple clang). The compiler removed the allocation and the fill (dead-store elimination: it deletes writes whose value can never be observed). `noise_volatile` keeps 1 call to `operator new` and the store loop.
  - GCC 13.3 (aarch64): both versions keep the call to `operator new` and the store loop (`str w2, [x3], 4` in the loop). So the same source is a real noise generator with one compiler and an empty function with another.
  - Habit: before you trust a stress or noise generator, look at its listing (or time it alone and check that the number fits the work). A `volatile` access is observable behaviour, so the loop must stay.
  - Holds for / re-check: counts hold for the three compilers named, at `-O2`. Other versions or flags may keep or delete these stores; that is why the listing is checked every time. `volatile` guarantees the accesses stay, not that they hit DRAM or evict anything useful.
</details>

---

## Close

Next: part 4, `lesson-06-practice-bank-04-call-costs.md` (about 24 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
