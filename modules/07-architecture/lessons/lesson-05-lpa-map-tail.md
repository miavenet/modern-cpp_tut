# Lesson 05 — LPA Lens: flat_map vs map, Read as a Distribution

**Time: about 25 minutes** (reading about 7, running one program about 2, cards about 16). Stop after any card; the driver resumes there. This lesson follows lesson 03 (concepts C5 and C6); do it after lesson 03, before or after lesson 04.

**Hook:** lesson 03's benchmark style prints one number per data structure. For a million-instrument price table it says `flat_map` is 5.9x faster than `std::map`. Per lookup, at p99.99, the gap was only 1.4x. And the one-number benchmark never saw the row that matters most: one `flat_map` insert took 160 times as long as a `map` insert.

**You will be able to:** turn a benchmark's samples into a tail percentile correctly, say what a median of means hides, and choose between `flat_map` and `map` by the operation on your latency path.

**Needs from earlier:** lesson 03 (concept C5, `mini_bench`'s median of 5 means; concept C6, `std::flat_map`).

## Pretest — commit your guess FIRST

### Card M7-LPA1 · Pretest
Q: A table of 1,000,000 prices, filled in random order. A median-of-5-means benchmark says `flat_map` lookups are 5.9x faster than `std::map`. Guess: timed one lookup at a time, is `flat_map`'s advantage at p99.99 bigger, about the same, or smaller than 5.9x? And how does one `flat_map` *insert* of a new key compare with one `map` insert?
<details><summary>Answer</summary>
Key:

  - Smaller: about 1.4x at p99.99 (6.1 µs against 4.4 µs, median of 7 runs). At p50 it was 4.7x.
  - An insert went the other way: `flat_map` about 197 µs per insert at p50, `map` about 1.2 µs. Inserting into the middle of a sorted vector moves on average half of the million elements.
</details>

---

## The tail and its mechanism

### CONCEPT C10  LPA lens: benchmark the distribution of the operation you actually run

The *Latency Prioritized Approach* (LPA) judges a path by its far tail (p99.99), not the mean. Every LPA choice answers three questions:

- **Default does / LPA does:** the default node map (`std::map`) puts every entry in its own heap node. LPA stores sorted keys and values contiguously (`std::flat_map`, or a sorted vector with `std::lower_bound`) when the latency path only *looks up*. And LPA benchmarks report percentiles of single operations, not a mean.
- **Why (mechanism):** a `map` lookup follows one pointer per tree level (a balanced tree of a million nodes is at least 20 levels deep), and nodes allocated at different times sit far apart, so many steps miss the cache. A `flat_map` lookup is a binary search over one array of keys: the same ~20 steps, but the first ones hit the same few hot cache lines every time. A *median of means* averages a million lookups into one number, then keeps the middle of five such numbers, so it cannot see any single slow lookup.
- **Gives up:** inserts and erases are O(n): every element after the slot moves. Pointers and iterators into a `flat_map` are invalidated by any insert. Building one by inserting one key at a time is O(n²); build from sorted data in one go.

**What we measured.** `lab/bench/map_tail.cpp` (build line on its fourth line). Course Mac: Apple M1 Max, macOS 27, Apple clang 21, `-O2`, one thread, no core pinning, shared machine (1-minute load average 8.7 to 9.2 across the seven runs, 5-minute 9.3 to 9.4). 1,000,000 keys inserted in shuffled order; 1,000,000 lookups of random existing keys; 1,000 inserts of new keys. Median [lowest–highest] of 7 runs (raw: `ingest/lpa/evidence/m7-map-tail-20261114-230312.txt` and its `.summary`).

**What is timed.** Both lookup views put the same compiler barrier (`escape`) after every lookup, so they differ only in the clocks: the median of means reads the clock twice per 1,000,000 lookups; the per-lookup rows read it twice per lookup (so each per-lookup sample includes the clock reads). The insert rows have no barrier because an insert changes the map. An earlier set of 7 runs, where the barrier sat inside the per-lookup rows only, gave 6.2x, 4.3x and 3.0x (`m7-map-tail-20261114-222735.txt`, load 19 to 23): the far-tail ratio moves a lot with the machine.

| Measurement | `std::map` | `std::flat_map` | ratio |
|---|---|---|---|
| median of 5 means (lesson 03 style) | 601 ns | 101 ns | 5.9x |
| lookup p50 | 591 ns | 125 ns | 4.7x |
| lookup p99 | 1,183 ns | 503 ns | 2.4x |
| lookup p99.99 | 6,143 ns [5,247–11,263] | 4,351 ns [1,151–4,479] | 1.4x |
| insert p50 (1,000 inserts) | 1,215 ns | 196,607 ns | 0.006x (160x *slower*) |

The insert rows have only 1,000 samples, so the harness prints p99.9 and p99.99 as `n/a` (it needs 10,000 and 100,000 samples). That is the harness refusing to make up a number.

What a median of means hid, three ways:

- *The spread inside each structure.* Each one's own p99.99 was about 10x (`map`) and 35x (`flat_map`) its p50.
- *That the advantage shrinks at the tail.* The far tail of both is mostly the machine (interrupts, preemption), which neither data structure can avoid. `flat_map` still won at every percentile, but not by 5.9x.
- *The operation it did not run.* The benchmark timed lookups only. If your latency path also inserts, `flat_map` turns a 1.2 µs operation into a 200 µs one.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `std::map` | O(log n) pointer-chasing lookup; O(log n) insert, stable iterators | Inserts and erases on the latency path | Lookup-only table, cold cache misses in the profile |
| `std::flat_map` | Contiguous binary-search lookup; O(n) insert | Built once (or rarely), read on every tick | Inserts on the hot path; built one key at a time |
| Median of means | One stable number; no tail at all | Comparing throughput | Used to claim a latency |

**Rule:** benchmark the operations your path really runs, one at a time, and report p50 to p99.99 with the sample count. Fine print: on a shared machine the far tail moves 5x between runs; repeat and show the spread.

### Card M7-LPA2 · Recall
Q: Why is a `std::map` lookup slower than a `std::flat_map` lookup on a large table, and why is a `flat_map` insert slower than a `map` insert?
   Answer format: one line; separate the two reasons with a semicolon.

<details><summary>Answer</summary>
Key:

  - Lookup: `map` chases a pointer per tree level (at least 20 for a million keys) through nodes scattered on the heap, so many steps are cache misses; `flat_map` binary-searches one contiguous array whose top levels stay in cache.
  - Insert: `flat_map` must shift every element after the new key's slot (O(n), about half a million moves here); `map` allocates one node and relinks a few pointers.
</details>

### Card M7-LPA6 · Fix it
Q: Worked first: the nearest-rank p99 of a sorted `s` is `s[ceil(0.99 * n) - 1]`; in whole numbers `s[(s.size() * 99 + 99) / 100 - 1]`, because adding (divisor minus 1) before an integer division rounds *up*. Now finish the p99.99 version; two numbers fill the blanks.
   ```cpp
   // Fragment: s is a sorted std::vector<std::uint64_t>, s.size() >= 1.
   s[(s.size() * ___ + ___) / 10000 - 1]
   ```
   Answer format: the two numbers in order, separated by a comma.
   The check builds your expression and compares it with the nearest rank for 11 sample counts from 1 to 1,000,000.

<details><summary>Answer</summary>
Key:

  - `9999, 9999`. The multiplier is 10000 - 1 (that is 0.9999), and the added blank is the divisor minus 1, so the division rounds up to the ceiling rank. The next card asks you to write the whole expression.
  - Swapping the last blank for 0 gives the floor rank: for n = 10,000 it returns `s[9999]` (the max) instead of `s[9998]`.
</details>

### Card M7-LPA3 · Build and check
Q: Your benchmark kept every lookup's time in a *sorted* `std::vector<std::uint64_t> s` (at least one sample). Write one expression for its nearest-rank p99.99: the smallest sample with at least ceil(0.9999 × n) samples at or below it.
   Answer format: one C++ expression on one line, using `s`.
   The check tries 11 sample counts from 1 to 1,000,000 (including 10,000 and 10,001) on sorted values that are not simply 1, 2, 3, ..., so an expression that only computes the rank does not pass, and it catches reads past the end. A trailing `;` is fine.

<details><summary>Answer</summary>
Key:

  - `s[(s.size() * 9999 + 9999) / 10000 - 1]`. In whole numbers, (n × 9999 + 9999) / 10000 is ceil(0.9999 × n): the rank. Subtract 1 for a zero-based index.
  - `s[s.size() * 9999 / 10000]` is a floor-based rank, one sample too high for n = 10,000 (it returns `s[9999]`, the max, instead of `s[9998]`). With n = 1 it is still in range, but it is a different statistic. `s[s.size() / 2]` is the median, the number that hid the tail. `s[std::ceil(0.9999 * s.size()) - 1]` also passes the check's sample counts; the whole-number form does not depend on how 0.9999 rounds in binary.
</details>

### Card M7-LPA4 · Apply
Q: An order-entry gateway keeps a symbol table (about 20,000 symbols, loaded at start-up, never changed during the day) and a table of open orders (about 50,000; an insert and an erase on most messages). Which container do you choose for each, `std::flat_map` or `std::map`?
   Answer format: one line, like `symbols: X; orders: Y`.

<details><summary>Answer</summary>
Key:

  - `symbols: flat_map; orders: map`. The symbol table is read-only on the latency path, so contiguous lookups win and the O(n) insert is paid once at start-up (built from sorted data). The order table inserts and erases on most messages; a `flat_map` there pays O(n) moves on every one (about 25,000 elements on average), which lands straight in the tail.
</details>

### Card M7-LPA5 · Recall
Q: Name three things a median-of-5-means benchmark cannot tell you about a latency path.
   Answer format: one line; separate the three with semicolons.

<details><summary>Answer</summary>
Key:

  - How slow single operations get: the spread inside one run (here p99.99 was 10x to 35x the p50), because a mean of a million lookups averages them away.
  - Whether the advantage holds in the tail: here 5.9x by means, 1.4x at p99.99.
  - The cost of operations it did not time, such as inserts (here 160x slower for `flat_map`). Also: how many samples back a claim, since one number has no sample count.
</details>

---

## Close

**Remember:** time single operations, report percentiles with the sample count, and benchmark every operation the path runs; `flat_map` wins lookups at every percentile here and loses inserts by 160x. **Tomorrow:** missed cards come back.
