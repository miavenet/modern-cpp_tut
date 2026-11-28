# Lesson 06 — Practice Bank, part 4 of 12 (optional): Percentiles, Bins and Keys

**Time: about 26 minutes (estimate)** for 4 cards (8 + 6 + 6 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: nearest-rank percentiles, half-open histogram bins, merging bins, and trace keys that collide.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB6 · Apply
Q: A service log holds N = 50,000 request latencies. Most requests take 1 ms; the k requests that miss a cache take 9 ms. Using the nearest-rank percentile (the q-th percentile is the sample at rank ceil(q*N) in ascending order), how many slow requests can the log hold before p99.9 shows 9 ms? Before p99 does? Then check your boundaries at k = 50, 51, 500, 501:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pctl_log.cpp -o "${TMPDIR:-/tmp}/pctl_log" && "${TMPDIR:-/tmp}/pctl_log"
   ```
   Answer format: two numbers (p99.9, then p99) and the general rule.
<details><summary>Answer</summary>
Key:

  - p99.9: ceil(0.999 x 50,000) = rank 49,950, so 50 samples sit above it. Up to 50 slow requests stay invisible; the 51st moves it (the run: k = 50 gives 1 ms, k = 51 gives 9 ms).
  - p99: rank 49,500, 500 samples above it, so it needs k > 500 (k = 500: 1 ms, k = 501: 9 ms).
  - The max shows the slow path for any k >= 1.
  - General rule: with nearest-rank, the q-th percentile moves only if k > N(1 - q). Before you call a percentile "clean", compare N(1 - q) with how many slow samples you expect.
  - Holds for / re-check: pure counting, so it holds on any machine. It depends on the percentile definition: other definitions (e.g. index floor(q*N), or interpolation) move the boundary by about one sample, so state which one your tool uses.
</details>

### Card M6-PB33 · Spot the trap
Q: A toy trace collector uses the sum of two function IDs as its path key. Its event sequence is {(2,5),(2,5),(5,2),(2,5),(5,2)}. A second collector keys on the full ordered pair. Predict the record counts and event counts of each. Run `path_identity.cpp`, then explain why a correct grand total cannot validate path attribution:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/path_identity.cpp -o "${TMPDIR:-/tmp}/path_identity" && "${TMPDIR:-/tmp}/path_identity"
   ```
   Answer format: records and events for each collector, then the trap, then the fix.
<details><summary>Answer</summary>
Key:

  - Sum-key collector: 1 record with 5 events. Full-pair collector: 2 records, (2,5) with 3 and (5,2) with 2.
  - Both account for all five events, yet one merges distinct routes. A total-count check misses this attribution failure.
  - For this toy collector, use the full pair as identity. In a hashed design, compare full keys when hashes match; a wider hash alone is not an exact-identity check.
  - A forced collision belongs in the collector's tests, along with a check of total events.
  - Holds for / re-check: Apple clang 21, C++20; a deterministic two-function trace model, no production profiler, no timing. Re-check on longer paths and recursion. The toy sum is deliberately poor and says nothing about any real profiler's collision rate.
</details>

### Card M6-PB35 · Spot the trap
Q: A histogram stores counts in half-open bins `[0,10)`, `[10,20)`, `[20,40)`; a half-open bin includes its lower bound and excludes its upper bound. Dataset A is `{1,2,3,4,19}`; dataset B is `{1,2,3,4,10}`. Predict both count arrays and exact p90 values, using nearest rank `ceil(0.9*N)` in sorted order. Can a report recover exact p90 from these counts alone? Implement the counts before checking the `quantile` mode of:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/histogram_contract.cpp -o "${TMPDIR:-/tmp}/histogram_contract" && "${TMPDIR:-/tmp}/histogram_contract" quantile
   ```
   Answer format: both count arrays, both exact p90 values, yes or no, then what a report should state.
<details><summary>Answer</summary>
Key:

  - Both histograms are `{4,1,0}`. Rank 5 gives exact p90 = 19 for A and 10 for B.
  - The stored summaries are identical although the exact answers differ. So counts alone cannot tell these p90 values apart.
  - Report p90's containing interval `[10,20)` (integer values 10 through 19 here), or label a representative point as an estimate. The midpoint or upper edge is not the recovered observation.
  - Holds for / re-check: invented bounded integer samples, fixed half-open bins, nearest-rank percentile; no measured latencies. Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`. Re-check interval conventions and rank definitions when changing histogram libraries. Exact quantiles require keeping more information.
</details>

### Card M6-PB36 · Spot the trap
Q: Worker X bins at `{0,8,16,32}`; worker Y bins at `{0,4,16,32}`, both using half-open intervals in the same units. X records one sample, 6; Y records two, 6 and 12. A collector adds their count arrays by position and labels the result with X's edges. Predict that result and compare it with binning all three raw samples using X's edges. Write a merge guard that refuses unequal edges; also test a same-edge merge (X plus a worker X2 with X's edges that records 6 and 12) so a guard that rejects everything cannot pass. Then check the `merge` mode of:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/histogram_contract.cpp -o "${TMPDIR:-/tmp}/histogram_contract" && "${TMPDIR:-/tmp}/histogram_contract" merge
   ```
   Answer format: the naive and pooled counts, both totals, then what the merge guard compares.
<details><summary>Answer</summary>
Key:

  - Naive addition gives `{1,2,0}`. Pooling the three raw samples under X's edges gives `{2,1,0}`.
  - Both totals are 3. A correct grand total does not validate the bucket meanings.
  - The program rejects X+Y and accepts X+X2, producing `{2,1,0}` for the latter. Compare the edge arrays before summing corresponding counters.
  - This fixture fixes units and interval convention. A real merge contract must fix those too, along with how out-of-range samples are treated.
  - Holds for / re-check: the same two toolchains and flags as card M6-PB35, bounded fixture counts with no overflow. This exercise rejects incompatible summaries; it does not redistribute counts between bins. Re-check compatibility when precision, units, edges or boundary conventions change.
</details>

---

## Close

Next: part 5, `lesson-06-practice-bank-05-wakeups.md` (about 25 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
