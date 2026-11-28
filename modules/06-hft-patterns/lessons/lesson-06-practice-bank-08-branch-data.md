# Lesson 06 — Practice Bank, part 8 of 12 (optional): Branches and Their Data

**Time: about 21 minutes (estimate)** for 2 cards (15 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: sorted against shuffled input, and a precomputed common case tested on held-out input.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB19 · Apply
Q: `sorted_shuffled.cpp` sums the bytes >= 128 in a 1 MiB array, 200 times, once with the array sorted and once shuffled (same values). Build A: plain `-O2`. Build B: `-O2 -DKEEP_BRANCH`, which puts an empty `asm volatile("")` inside the `if`. First read the `sum_big` listing of each build and say whether a per-element branch survives. Then predict for each build whether shuffled is slower than sorted, and run each three times:
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o -
   clang++ -std=c++20 -O2 -DKEEP_BRANCH -S modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_a" && clang++ -std=c++20 -O2 -Wall -Wextra -DKEEP_BRANCH modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_b"
   for r in 1 2 3; do for b in ss_a ss_b; do "${TMPDIR:-/tmp}/$b" sorted; "${TMPDIR:-/tmp}/$b" shuffled; done; done
   ```
   Answer format: what each listing shows, the direction per build, then the trap.
<details><summary>Answer</summary>
Key:

  - Listing: Build A's `sum_big` is vectorized (16 `uaddw`-family SIMD adds, no `tbz`); Build B's has no `uaddw` and a per-byte `ldrsb` + `tbz` test-and-branch.
  - Build B: shuffled was slower than sorted in 3 of 3 runs. Build A: no consistent order penalty at the printed precision. Grade branch presence and direction only; no magnitude is part of the key (one machine, varies with load).
  - So "sorted vs random input" only exposes branch prediction if the branch survives compilation. Check the listing first.
  - Trap: without the `asm volatile` clobber in the repeat loop, build A printed `0.000` ns per element and the listing had a single `bl sum_big` in `main`, outside the 200-repetition loop: the compiler hoisted the call and reused the result (so the printed value is rounded amortised output, not literally no work). The checksum was still right; only the time showed the problem.
  - Holds for / re-check: direction only, M1 Max, Apple clang 21 `-O2`, 3 process runs. Other compilers or flags may if-convert differently (or not vectorize); re-check the listing and the direction on the target.
</details>

### Card M6-PB32 · Apply
Q: A decoder precomputes the result for its most common opcode. `heldout_routes.cpp` chooses between opcodes 2 and 9 using training input {2,2,2,2,2,2,2,9}, then counts special-path hits on a separate input {2,9,9,9,9,9,2,9}. Other opcodes use the correct fallback. Predict the chosen opcode, both hit counts and the result mismatches. Run it before deciding whether the training hit count justifies deploying that specialization:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/heldout_routes.cpp -o "${TMPDIR:-/tmp}/heldout_routes" && "${TMPDIR:-/tmp}/heldout_routes"
   ```
   Answer format: chosen opcode, the hit counts and mismatches, then one line on what the held-out input shows.
<details><summary>Answer</summary>
Key:

  - It chooses 2: training hits=7 of 8, held-out hits=2 of 8. Specializing 9 instead would give 6 held-out hits.
  - The 7-to-2 drop comes from the different input distributions. The held-out input stands in for the deployed feed; holding it out is what lets the shift show, it does not cause it.
  - The result mismatch count is 0: passing correctness does not imply the specialized path stays common.
  - Keep the evaluation input out of the selection step. If you use its six-hit result to retune the candidate to 9, it has become training data and you need another held-out evaluation.
  - These are source-level route counts, not instruction counts or a speed ranking; the compiler can simplify this toy calculation.
  - Holds for / re-check: a deterministic toy decoder (Apple clang 21, C++20). Held-out means input not used to select the candidate. Counts change with the input distribution; after a feed change, check correctness and path frequency separately.
</details>

---

## Close

Next: part 9, `lesson-06-practice-bank-09-prefetch-forms.md` (about 23 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
