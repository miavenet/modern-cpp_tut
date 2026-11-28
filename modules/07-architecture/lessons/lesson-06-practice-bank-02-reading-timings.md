# Lesson 06 — Practice Bank, part 2 of 11 (optional): Reading Timings

**Time: about 30 minutes (estimate)** for 4 cards (15 + 5 + 5 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: how steady a timing is, and ratios against absolute costs.

Hook: A custom timestamp hash can create long collision chains; predict them before starting the timer.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB6 · Apply
Q: A cache of recent events is a `std::unordered_map` keyed by nanosecond timestamps. Someone wrote a custom hash, `ts / 1'000'000` ("hash at millisecond granularity"). The program builds 2^19 keys spaced about 1.5 us apart and loads them into two maps: one with `std::hash<uint64_t>`, one with the custom hash. For each map it first prints the bucket counts (`bucket_count()`, how many buckets are non-empty, and the largest `bucket_size()`), then times insert and a shuffled lookup of every key, 7 reps. Before running, predict the non-empty count and the largest bucket for the custom hash from the key spacing alone.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/hashq.cpp -o "${TMPDIR:-/tmp}/hashq" && "${TMPDIR:-/tmp}/hashq"
   ```
   Answer format: your two predicted numbers with the arithmetic, then what the timings showed.
<details><summary>Answer</summary>
Key:

  - Prediction from arithmetic: 2^19 keys x 1.5 us cover about 786 ms, so the custom hash has only about 787 distinct values; each millisecond holds about 1,000,000 / 1500 = 667 keys.
  - Measured (identical in every run): custom hash 787 non-empty buckets out of 823,117, largest bucket 667. `std::hash`: 388,181 non-empty, largest 7. The counts show the bug without any timing.
  - Times agree with the counts in direction: in every run here the custom hash's lookup and insert medians were both slower than the `std::hash` ones. The milliseconds and the ratios change with machine and load and are not part of the Key; a different ratio, or an inconclusive timing comparison you report as such, does not lower the grade.
  - Technique: inspect the bucket-size distribution (`bucket_count`, `bucket_size`) first. It is cheap, deterministic and machine-independent, and it explains the timing.
  - Holds for / re-check: the counts depend only on the keys, the hash and the library's bucket-count policy (libc++ here chose 823,117 buckets); another library will choose other bucket counts, but the custom hash still gives about 787 distinct values. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ chose 712,697 buckets; the custom hash still gave 787 non-empty, largest 667. The timings are this machine's. Re-check by printing the same counts on the target.
</details>

### Card M7-PB7 · Spot the trap
Q: A write-up compares two order-book layouts from one run each and reports "layout B is 1.5x slower than A". First see how steady a timing is: run the hash-table program from card M7-PB6 alone, then two copies at once, two or three rounds of each, and compare the min/median/max columns and the bucket counts:
   ```sh
   "${TMPDIR:-/tmp}/hashq"; "${TMPDIR:-/tmp}/hashq" & "${TMPDIR:-/tmp}/hashq" & wait
   ```
   (Build it first with the card M7-PB6 command if needed.) Why is the write-up not evidence yet, and what is the minimum you would ask for?
   Answer format: why in one or two lines, then the list of what you would ask for.
<details><summary>Answer</summary>
Key:

  - Same binary, same keys, same machine, 7 reps per cell. The bucket counts and the checksum were identical in every run, alone or paired; the times were not. With a second copy running, the timings changed with no change to the code, and by how much differed from round to round: in some rounds the `std::hash` medians barely moved, in others they rose a lot. So no fixed size of the effect is given here; your rounds will show their own.
  - So machine state alone moves the timings, by an amount you cannot know from one run, and a single run per side cannot show it. Without several runs and their spread you cannot tell a real 1.5x from noise; the deterministic counts are what stayed fixed.
  - Ask for: several runs per configuration, a median plus a spread (min/max or IQR), fixed inputs (seeded engine), the machine state (pinning, other load), and a deterministic counter that does not depend on timing, such as instructions retired or allocations made.
  - Holds for / re-check: the spread sizes are this desktop Mac's, for this load; spreads vary from run to run. Re-check by printing the spread every time.
</details>

### Card M7-PB19 · Recall
Q: Two request handlers get the same fixed 2 ms of extra work (say a new auth check). Handler A takes 4 ms before and 6 ms after. Handler B takes 20 ms before and 22 ms after. A report says "A slowed by 50%, B only 10%, so B copes better with the change." What is wrong with that conclusion, and what should the report show? Then: for what kind of change would a percentage be the right summary?
   Answer format: what is wrong, what to report, then when a percentage fits and how to tell.
<details><summary>Answer</summary>
Key:

  - Both handlers lose exactly 2 ms per request. The percentage divides one fixed cost by each handler's own baseline, so the shorter handler always looks "worse" for the same absolute loss.
  - After the change A (6 ms) is still faster than B (22 ms). The change did not make B the better handler.
  - Report the absolute time before and after and the absolute difference. A percentage is the right summary when the extra cost scales with the work, for example a build that makes every instruction 10% slower; then A would lose 0.4 ms and B 2 ms. To tell the two cases apart, measure two input sizes: a constant difference means a fixed cost, a constant ratio means a scaling cost.
  - Holds for / re-check: plain arithmetic (2/4 = 50%, 2/20 = 10%, 6 < 22; 10% of 4 = 0.4 and 10% of 20 = 2), so it holds for any machine. The numbers are made up for the exercise.
</details>

### Card M7-PB37 · Spot the trap
Q: Two synthetic files contain 900 and 100 raw bytes. Codec A stores them in 100 and 100 bytes; codec B stores them in 300 and 10 bytes. No timing is involved. Define compression ratio as raw bytes divided by stored bytes. Predict which codec wins if you average the two file ratios, and which wins for the combined archive. Write a calculator that prints both summaries, then compare with:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/ratio_totals.cpp -o "${TMPDIR:-/tmp}/ratio_totals" && "${TMPDIR:-/tmp}/ratio_totals"
   ```
   Answer format: both summaries for each codec, the winner for the archive, then how to aggregate.
<details><summary>Answer</summary>
Key:

  - Arithmetic mean: A = (9+1)/2 = 5; B = (3+10)/2 = 6.5, apparently favouring B.
  - Pooled ratio: A = 1000/200 = 5; B = 1000/310 ≈ 3.225806. A stores this archive in fewer bytes.
  - For the archive question, sum raw bytes and stored bytes separately, then divide. Equivalently, weight each file's ratio by its stored bytes, not equally.
  - An unweighted mean answers a different question. Keep the underlying counts so readers can rebuild the summary that fits their workload.
  - Holds for / re-check: an invented byte-count fixture; no compressor was run and no codec speed or real compression ranking is claimed. Counts and arithmetic checked with Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`. Recompute when file weights change; include any required archive metadata in the stored-byte total.
</details>

---

## Close

Next: part 3, `lesson-06-practice-bank-03-fair-setup.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
