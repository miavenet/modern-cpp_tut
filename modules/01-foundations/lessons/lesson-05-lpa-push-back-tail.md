# Lesson 05 — LPA Lens: The One push_back That Takes 43 Milliseconds

**Time: about 26 minutes** (reading about 7, the hands-on lab about 10, cards about 9). The lab is resumable: your work file stays where you left it.

**Hook:** two million `push_back`s into a `std::vector`. The median batch takes one clock tick in every run. The slowest batch takes 43 *milliseconds*: a million times the median, inside one call to `push_back`. One keyword cuts that by 5x. One more line cuts it by about 1,000x.

**You will be able to:** read a latency histogram for its tail, explain which growth step lands there and why `noexcept` changes its size, move the growth out of the timed path, and say what that costs you.

**Needs from earlier:** lesson 01 (concept C2, `noexcept` and vector growth). This lesson uses the course's latency histogram `modules/06-hft-patterns/lab/measure/lat_hist.hpp`; module 06 lesson 03 explains it in full, but nothing here needs that lesson first.

## Pretest — commit your guess FIRST

### Card M1-LPA1 · Pretest
Q: 2,000,000 orders, each holding a 40-character `std::string`, are moved one by one into an empty `std::vector`, timed per 16 `push_back`s. Run A's order type has a move constructor *without* `noexcept`. Run B's has `noexcept`. Run C called `reserve(2'000'000)` before the clock started. Guess the slowest batch (the max) of each run, roughly: microseconds or milliseconds?
<details><summary>Answer</summary>
Key:

  - A: about 43 ms. B: about 8 ms. C: about 45 µs. (Median of 7 runs on the course Mac.)
  - All three medians were one clock tick (42 ns) in every run. The median cannot see growth at all.
  - A's worst batch held the last growth step: about a million old elements *copied*, each copy allocating a new string. B *moved* them instead, about 5x cheaper, but still a million moves inside one `push_back`. C never grew.
</details>

---

## The tail and its mechanism

### CONCEPT C17  LPA lens: pay for growth before the clock starts

The *Latency Prioritized Approach* (LPA) aims for the lowest latency together with high throughput, and judges by the far tail (p99.99), not the mean. Every LPA choice answers three questions:

- **Default does / LPA does:** the default lets the vector grow as orders arrive. LPA marks the element's move `noexcept` *and* reserves (and touches) the peak's worth of capacity at start-up.
- **Why (mechanism):** when the buffer is full, one `push_back` allocates a buffer twice as big and relocates every old element. That is O(n) work inside one O(1)-on-average call. Without `noexcept` the vector *copies* (lesson 01, C2), and here every copy allocates a string. Even after `reserve`, the first write to each fresh 16 KB page of the new buffer takes a *page fault*: the OS maps the page before the write can finish.
- **Gives up:** memory for the worst case, held from start-up even in quiet hours; start-up time; and you must know the peak (past it, the vector grows again).

**What we measured.** `lab/solution/push_tail.cpp`. Course Mac: Apple M1 Max, macOS 27, Apple clang 21, `-O2`, one thread, no core pinning, shared machine (load average about 11). 125,000 samples per run, each one batch of 16 `push_back`s. Median [lowest–highest] of 7 runs, ns. Copies and heap calls were identical in every run; minor page faults varied by at most 2 (copy-growth 14,647–14,649, move-growth 10,014–10,015; the other two rows did not vary). The table shows the median.

| Run | p50 | p99.99 | max | copies while timed | heap calls while timed | page faults while timed |
|---|---|---|---|---|---|---|
| copy-growth (no `noexcept`) | 42 | 49,151 [43,007–57,343] | 42.9 ms [41.7–46.4 ms] | 2,097,151 | 2,097,173 | 14,648 |
| move-growth (`noexcept`) | 42 | 30,207 [27,135–41,983] | 8.3 ms [8.1–8.8 ms] | 0 | 22 | 10,014 |
| reserve only | 42 | 21,503 [19,455–26,111] | 44,583 [28,542–340,250] | 0 | 0 | 4,883 |
| prepared (reserve + touch) | 42 | 6,655 [3,647–7,295] | 28,542 [19,167–237,417] | 0 | 0 | 0 |

What to read off it:

- *Growth lives only in the max.* There are only 22 growth steps among 125,000 samples, under 0.02%. The biggest ones are the max.
- *`noexcept` changed copies to moves.* Copies dropped from 2,097,151 (every element, at every growth step) to 0, and the max fell about 5x. It did *not* remove the growth: 22 heap calls, and a million moves in the last step.
- *`reserve` removed growth, not first touch.* With room reserved, heap calls fell to 0 and the max fell about 190x below move-growth. But 4,883 page faults were still paid inside the timed loop, one about every 26 batches (3.9% of samples), so p99.99 stayed at about 21 µs.
- *Touching the pages first removed the faults.* `resize(peak)` then `clear()` writes every page before the clock and keeps the capacity. Page faults fell to 0 and p99.99 fell about 3x more. That is a separate cost from growth, and reserve alone does not pay it.

Trust the counters and the ratios more than the exact nanoseconds. The clock ticks every 41.7 ns, which is why each sample is a batch of 16. The max swung by up to about 12x between runs (the OS taking the core away), so it is the least repeatable number shown.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Grow as you go | Amortised O(1); one call in many is O(n) | Code off the latency path | A max thousands of times the median |
| `noexcept` move | Growth moves instead of copying; still O(n) at a step | Every type with a cheap, non-throwing move | Copy counter climbs while a vector grows |
| `reserve` + touch at start-up | No growth or faults while timed; peak memory held | A latency path with a known peak | Resident memory far above typical use |

**Rule:** make moves `noexcept`, then reserve the peak (and touch it) before the clock starts. Fine print: a `noexcept` move that *can* throw ends the program with `std::terminate`.

### Card M1-LPA2 · Build and check
Q: Do the lab: in `modules/01-foundations/lab/work/push_tail.cpp`, add the one keyword `Order` needs and write `prepare_for_peak`. Run it, then submit that path.

   Step 0 (from the tutorial root; the `cp` OVERWRITES an earlier work copy):
   ```sh
   mkdir -p modules/01-foundations/lab/work && cp modules/01-foundations/lab/starter/push_tail.cpp modules/01-foundations/lab/work/
   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/01-foundations/lab/work/push_tail.cpp -o "${TMPDIR:-/tmp}/push_tail" && "${TMPDIR:-/tmp}/push_tail"
   ```
   Before you edit, **predict** the starter's `move-growth` copy counter, and whether its `prepared` row beats `reserve only`. Then do the TODOs, rebuild, and compare. Check with
   `printf '%s\n' modules/01-foundations/lab/work/push_tail.cpp | modules/01-foundations/checks/push_tail.check.sh`.
   It grades the copy and heap-call counters, never a time.
Hint: one keyword on the move constructor; for the function, `clear()` keeps a vector's capacity.

<details><summary>Answer</summary>
Key:

  - `Order(Order&& o) noexcept : ...` on the move constructor.
  - `book.reserve(peak); book.resize(peak); book.clear();` in `prepare_for_peak`. `reserve` is the one statement the check needs (zero heap calls). `resize` then `clear` also touches every page, and the `prepared` row's page-fault counter drops to 0.
  - Success: `move-growth` shows `copies while timed = 0`, `prepared` shows 0 copies and 0 heap calls, and the program prints `RESULT: PASS`. Your exact nanoseconds will differ.
  - The check also calls your function with peaks of 1, 777, 5,000 and 2,010,000. Putting `noexcept` on the copy constructor instead, reserving half the peak, reserving a fixed `kPeak`, or reserving a different vector all fail.
</details>

### Card M1-LPA3 · Recall
Q: In the move-growth run, p50 stayed at one clock tick while the max was about 8 ms. Name the mechanism inside that one slow batch, and the two counters that told `noexcept` apart from `reserve`.
   Answer format: one line; separate the mechanism and the counters with a semicolon.

<details><summary>Answer</summary>
Key:

  - Mechanism: a growth step. One `push_back` found the buffer full, allocated one twice as big, and moved every old element (about a million) into it.
  - Counters: copies while timed (2,097,151 without `noexcept`, 0 with it) and heap calls while timed (22 growth steps with `noexcept` alone, 0 after `reserve`).
</details>

### Card M1-LPA4 · Apply
Q: A gateway keeps live orders in a `std::unordered_map<std::uint64_t, Order> live;`. On a busy open, an insert now and then takes milliseconds while every bucket is rebuilt. You expect at most 500,000 live orders. Give the one statement you run at start-up.
   Answer format: one C++ statement on one line.

<details><summary>Answer</summary>
Key:

  - `live.reserve(500'000);` It sizes the bucket array for that many elements up front, so no insert up to the peak triggers a rehash (the same growth-step mechanism as the vector). `live.rehash(...)` with a bucket count of at least 500,000 / `max_load_factor()` does the same job.
  - It does not pre-allocate the nodes: each insert still allocates one. Module 05's pool removes that cost too.
</details>

### Card M1-LPA5 · Recall
Q: Name three things the "reserve and touch the peak at start-up" vector gives up compared with growing as you go.
   Answer format: one line; separate the three with semicolons.

<details><summary>Answer</summary>
Key:

  - Memory: the peak's capacity is held all day (here 2,000,000 × 40 bytes = 80 MB), even when the book is small.
  - Start-up time: the allocation and every first-touch page fault are paid before you go live.
  - You must know the peak: past it, the vector grows again (and the tail comes back). With a wrong `noexcept` promise, a throwing move ends the program.
</details>

---

## Close

**Remember:** growth is O(n) inside one call, so it lives in the max; `noexcept` makes it a move, `reserve` removes it, touching the pages removes the faults. **Tomorrow:** missed cards come back. **Next:** module 02, compile-time code.
