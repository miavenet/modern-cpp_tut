# Lesson 06 — Practice Bank, part 1 of 11 (optional): Check the Result Before the Clock

**Time: about 29 minutes (estimate)** for 5 cards (10 + 3 + 6 + 3 + 7 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: correctness checks before timing, and ledgers and benchmark rows that must match.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB2 · Apply
Q: Build a correctness check before any timing. Three functions answer "how many values in this sorted array are strictly less than x?": a reference that uses a prefix-count table, a fast one that uses `std::lower_bound`, and one that uses `std::upper_bound`. All three get the same 100,000 seeded queries and fold their answers into a checksum. Predict which ones match the reference, then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/rank_check.cpp -o "${TMPDIR:-/tmp}/rank_check" && "${TMPDIR:-/tmp}/rank_check"
   ```
   What do you do with a contestant whose checksum differs, even if it is the fastest?
   Answer format: which pass and why, then what you do with a failing contestant.
<details><summary>Answer</summary>
Key:

  - The `lower_bound` version matches (PASS); the `upper_bound` version does not (FAIL). `upper_bound` returns the first element greater than x, so it also counts the elements equal to x; with values drawn from 0..999 nearly every query value is present.
  - A contestant that fails did different work. Its time is meaningless; discard the run and fix it first.
  - Make the reference a different, simple algorithm, so it is unlikely to share the contestant's bug. Fix the seed so all contestants get the same input. Run the check every time you change the size, the code or the machine you test on.
  - Holds for / re-check: the method holds anywhere. The checksum values depend on the seed and the PRNG; `std::mt19937_64` is fully specified by the standard and the answers are counts, so they should match on any conforming library.
</details>

### Card M7-PB3 · Recall
Q: Before timing several implementations of the same operation against each other, what check must every run pass, and why is a mixed workload (several operations folded into one number) hard to interpret? This is the short recall check after PB2; answer from memory. Use `:quit` to pause.
   Answer format: the check in one line, then why a mixed number misleads, with an example.
<details><summary>Answer</summary>
Key:

  - A correctness check: every contestant answers the same seeded queries and produces the same checksum as a simple reference implementation; a mismatch voids the run.
  - A mixed number can hide opposite effects. Example (arithmetic): implementation X takes 10 ns per insert and 30 ns per erase, Y 20 and 20. On a 50/50 mix both average 20 ns, a tie, yet X is 2x faster at inserts and 1.5x slower at erases. Report each operation in its own column.
  - Holds for / re-check: a method; it holds for any language or machine. The correctness check is the one in card M7-PB2.
</details>

### Card M7-PB30 · Apply
Q: A storage test retries six logical writes. Their attempt counts are {1,3,1,2,4,2}; only each final attempt succeeds. Before opening `retry_ledger.cpp`, predict logical writes, attempts, completions and retries. Run it in `complete` mode, then check both attempts = completed + failed and completed = logical writes. What does each identity establish?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/retry_ledger.cpp -o "${TMPDIR:-/tmp}/retry_ledger" && "${TMPDIR:-/tmp}/retry_ledger" complete
   ```
   Answer format: the four counts, then one line per identity.
<details><summary>Answer</summary>
Key:

  - Logical writes=6, attempts=13, completed=6, failed attempts=7. Here every failed attempt is retried, so retries=7.
  - attempts = completed + failed checks that every attempt has an outcome: 13=6+7. completed = logical writes checks that all six logical writes completed. Record both before interpreting a throughput result.
  - Holds for / re-check: a counted simulation (Apple clang 21, C++20); no I/O or timing is measured. Re-check the ledger when you introduce dropped requests or partial completions.
</details>

### Card M7-PB31 · Spot the trap
Q: A second test run retries five logical writes with attempt counts {2,1,3,1,2}; at first each final attempt succeeds. A deadline change lets the last attempt of the three-attempt write fail permanently. An engineer treats a passing attempt-partition check (attempts = completed + failed) as proof that all writes completed. Compute the counters on paper, then check the `drop` mode of `retry_ledger.cpp`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/retry_ledger.cpp -o "${TMPDIR:-/tmp}/retry_ledger" && "${TMPDIR:-/tmp}/retry_ledger" drop
   ```
   Which identity catches the change?
   Answer format: attempts, completions and failed attempts, then which identity holds and which fails, then what to report.
<details><summary>Answer</summary>
Key:

  - Attempts 9; completions 4; failed attempts 5.
  - attempts = completed + failed still holds (9=4+5), but completed = logical writes no longer holds (4 versus 5).
  - Report the dropped logical write separately. A partition check alone cannot certify delivery.
  - Holds for / re-check: both cases compiled and run with Apple clang 21, C++20, -O2 -Wall -Wextra. A failure here means an unsuccessful attempt; a drop means a logical write with no successful attempt. Re-check these definitions when retries can partially commit.
</details>

### Card M7-PB33 · Apply
Q: Two benchmark workers emit synthetic operation-count rows in completion order. The current reference rows (variant,size,count) are (red,4,12), (blue,4,8), (red,8,24), (blue,8,16). Worker A emits (red,8,24), then (red,4,12); worker B emits (blue,8,15), then (blue,4,8). Worker B has one stale cell from an earlier run. The merger alternates A then B, keeping each worker's order. Predict how many count mismatches a positional comparison finds, versus a comparison keyed by (variant,size). Run the checker. Which cell is stale?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/report_join.cpp -o "${TMPDIR:-/tmp}/report_join" && "${TMPDIR:-/tmp}/report_join"
   ```
   Answer format: both mismatch counts and the stale cell, then the rule for comparing merged results.
<details><summary>Answer</summary>
Key:

  - Positional comparison reports 4 count mismatches. Keyed comparison reports exactly 1: blue, size=8, merged=15, raw=16.
  - A row's position reflects worker completion and merge order, not experimental identity. Join on every dimension that identifies a cell before comparing values.
  - When you extend this checker, test duplicate keys and missing or unknown cells explicitly; this fixture does not exercise those guard branches. Its exit 0 means the deliberately bad fixture produced the expected one mismatch; it does not approve the merged worker output.
  - In a real worker pipeline, reject the merged output when keyed_mismatches is nonzero.
  - Holds for / re-check: an invented integer-count fixture (Apple clang 21, C++20); no timings. In a real experiment add compiler, target, input seed and the other varying dimensions to the key; rerun when the worker-output schema changes.
</details>

---

## Close

Next: part 2, `lesson-06-practice-bank-02-reading-timings.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
