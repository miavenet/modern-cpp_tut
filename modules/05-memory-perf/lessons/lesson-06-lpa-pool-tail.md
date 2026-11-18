# Lesson 06 — LPA Lens: Pay Before the Clock Starts

**Time: about 28 minutes** (reading about 8, the hands-on lab about 10, cards about 10). The lab is resumable: your work file stays where you left it.

**Hook:** same pool, same code, same burst of orders, same median: 83 ns. One version's 99.9th percentile is about 400 ns; the other's is about 17,000 ns. The only difference is *when* the pool does work it has to do anyway.

**You will be able to:** read a latency histogram for its tail, name the mechanism that moved it, move that cost out of the timed path, make a missed plan fail loudly, and say what that costs you.

**Needs from earlier:** lesson 05's finished `fixed_pool` (`lab/work/pool_lab.cpp` prints `RESULT: PASS`), and lesson 01's `null_memory_resource()`.

## Pretest — commit your guess FIRST (no fair reading ahead)

### Card M5-LPA1 · Pretest
Q: Two runs push 2,000,000 orders into an empty `std::pmr::list<Order>` on lesson 05's pool, timing every 16 `push_back`s. Run A lets the pool fetch a new chunk whenever it runs dry. Run B fetched every chunk it will need *before* the clock started. Guess two ratios, A divided by B: the median (p50), and the 99.9th percentile (p99.9).
<details><summary>Answer</summary>
Key:

  - p50: about 1x. Both medians were 83 ns in all 7 runs on the course Mac.
  - p99.9: about 40x. Run A's p99.9 was about 16,900 ns, run B's about 420 ns (median of 7 runs, 40x; an earlier 7-run session, which kept its raw file, gave 41x).
  - The average hides this. Most samples never meet a refill, so the median cannot see it. The rare samples that do meet one make up the tail.
</details>

---

## The tail and its mechanism

### CONCEPT C16  LPA lens: move one-off costs out of the timed path

**What LPA means here.** This course calls it the *Latency Prioritized Approach* (LPA). You aim for the lowest latency together with high throughput, and you judge by the far tail (p99.99), not the mean. The goal is to avoid samples that are 10x, 100x or 1000x slower than the typical one. Every LPA choice answers three questions:

- **Default does / LPA does:** the default pool fetches chunks *when it runs out*, inside whatever operation happens to be running. The LPA version fetches the peak's worth *at start-up*, then serves every timed request from its free list.
- **Why (mechanism):** a refill is one upstream allocation plus the *first touch* of every block as it is pushed onto the free list. On this Mac, memory is handed out in 16 KB pages, and the first write to each fresh page takes a *page fault*: the OS has to map the page before the write can finish. One unlucky `push_back` pays all of that.
- **Gives up:** memory sized for the worst case, held all day; start-up time; and you must know your peak.

**Words first.** A *percentile* answers "how slow is the slowest x% of samples?" p99.9 is the value that 99.9% of samples are at or below. The *tail* is the high percentiles: p99, p99.9, p99.99 and the max. One rule does most of the work:

> A rare cost that hits a fraction *f* of samples shows up only in the percentiles above 1 − *f*.

In this lab, 2,000,000 orders at 4,096 blocks per chunk need 489 refills, so they hit at most 489 of 125,000 samples, which is 0.39%. So p99 (the slowest 1%) cannot see them, but p99.9 (the slowest 0.1%) is made of them.

**What these numbers are, honestly.** They come from the course Mac: an Apple M1 Max, macOS 27, Apple clang 21 with `-O2`. One thread, no core pinning (macOS offers only hints), on a shared machine, 125,000 samples per run. The values are median [lowest to highest] over 7 runs, in ns per batch of 16 `push_back`s (raw output: `ingest/lpa/evidence/m5-pool-tail-20261114-230017.txt` and its `.summary`).

| Run | p50 | p99 | p99.9 | p99.99 | heap calls while timed | page faults while timed |
|---|---|---|---|---|---|---|
| default heap | 335 [335–335] | 1,631 [1,599–1,727] | 3,711 [3,327–5,375] | 14,591 [8,703–32,767] | 2,000,000 | ~5,900 |
| pool, lazy | 83 [83–83] | 125 [125–211] | 16,895 [16,383–17,407] | 22,527 [20,479–27,647] | 499 | ~4,900 |
| pool, prepared | 83 [83–83] | 167 [125–211] | 423 [375–463] | 591 [503–14,847] | 0 | 0 |

The 499 heap calls are 489 chunk fetches plus 10 for the pool's own list of chunks, which grows 1, 2, 4, ... 512 entries (489 fits in 512), one heap call per growth. An earlier set of 7 runs (`m5-pool-tail-20261114-221049.txt`) gave the same p50s and counters, lazy and prepared p99.9 of 17,407 and 423, but p99.99 values of 33,791 (lazy) and 1,503 (prepared): 1.5x and 2.5x the table's. The far tail moves with whatever else the machine is doing. The counters did not move at all.

Trust the ratios and the counters more than the exact nanoseconds. The steady clock here ticks in steps of about 42 ns, so 83 ns means "two ticks". That is why the lab times batches of 16 rather than one call at a time. Percentiles are bucketed and read at most about 3% high. The max swung by up to about 8x between runs (the OS preempting the thread, interrupts), so it is the least repeatable number on the page.

**A side result that matters.** In a separate probe, warming the *general heap* the same way (fill it, free everything, then time it) removed its page faults (about 5,900 down to 4). Its p99 did not get better (median 1,567 ns cold, 3,007 ns warm, over 5 runs of a slightly simpler harness whose heap median was 211 ns). The heap's own bookkeeping is hidden from you, so you cannot prepare it. You *can* prepare your own pool. That is the LPA move.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| General heap | Every request is a heap call; its tail is outside your control | Code off the latency path | Heap calls counted inside the timed region |
| Lazy pool | Fast after warm-up; each refill lands inside some request | Throughput jobs that don't care about the tail | p50 is flat, but p99.9 jumps by tens of times |
| Prepared pool | Zero heap calls while timed; worst-case memory held | A latency path with a known peak | Resident memory grows; peak is a guess |

**Rule:** a cost you must pay anyway goes *before* the clock starts. Fine print: only up to the peak you planned for.

### Card M5-LPA2 · Build and check
Q: Do the lab: write the two statements of `prepare_for_peak` in `modules/05-memory-perf/lab/work/pool_tail.cpp`, run it, then submit that path.

   Step 0 (from the tutorial root; your lesson-05 `pool_lab.cpp` must already be in `lab/work/`; the `cp` OVERWRITES an earlier `pool_tail.cpp`):
   ```sh
   cp modules/05-memory-perf/lab/starter/pool_tail.cpp modules/05-memory-perf/lab/work/
   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/05-memory-perf/lab/work/pool_tail.cpp -o "${TMPDIR:-/tmp}/pool_tail" && "${TMPDIR:-/tmp}/pool_tail"
   ```
   Before you edit, **predict** the starter's `pool, prepared` row: its heap calls while timed, and whether its p99.9 beats `pool, lazy`. Then fill the TODO, rebuild, and compare the two `pool` rows. Check with
   `printf '%s\n' modules/05-memory-perf/lab/work/pool_tail.cpp | modules/05-memory-perf/checks/pool_tail.check.sh`.
   It grades the two counters, never a time.
Hint: the pool keeps every block it ever fetched, and `clear()` gives the nodes back to it.

<details><summary>Answer</summary>
Key:

  - `for (std::size_t i = 0; i < peak; ++i) book.push_back(Order{i, 100.0, 1, 'B'});` then `book.clear();`. Push the peak through *this* book, then hand every node back to the pool.
  - Success: the `pool, prepared` row shows `heap calls while timed = 0, minor page faults = 0`, and the program prints `RESULT: PASS`. Its p50 matches the lazy row and its p99.9 is far lower. Your exact nanoseconds will differ.
  - The check also calls your function with peaks of 1, 777, 5,000 and 2,010,000. It requires the book to be empty afterwards and no heap call while that many orders are pushed. Warming a different list on the default heap, warming half the peak, or skipping `clear()` all fail.
</details>

### Card M5-LPA3 · Recall
Q: In the lazy run, p50 stayed at 83 ns while p99.9 rose to about 17 µs. Name the two-part mechanism inside the timed region, and the two counters that exposed it.
   Answer format: one line; separate the mechanism and the counters with a semicolon.

<details><summary>Answer</summary>
Key:

  - Mechanism: a refill. That is an upstream (heap) chunk allocation, plus first-touch page faults as each block of the fresh chunk is written onto the free list. Both landed inside one unlucky timed batch.
  - Counters: heap calls while timed (499 lazy: 489 chunk fetches plus 10 growth steps of the chunk list; 0 prepared) and minor page faults while timed (about 4,900 lazy, 0 prepared).
</details>

### Card M5-LPA4 · Apply
Q: A risk check runs fine except that 1 call in 400 rebuilds a cache and takes 50 µs. You have 1,000,000 samples. Which is the *lowest* of p50, p99, p99.9, p99.99 that shows the 50 µs?
   Answer format: one percentile, written like `p99`.
Stretch: Under this course harness's rule (at least 10 samples above the rank), how many samples do you need before p99.99 is worth reporting?

<details><summary>Answer</summary>
Key:

  - p99.9. One call in 400 is 0.25%, so the slow samples are the slowest 0.25%. p99 is decided by the slowest 1%, and 99.75% of samples are fast, so p99 lands on a fast one. p99.9 is decided by the slowest 0.1%, and those are all slow.
</details>
<details><summary>Stretch answer</summary>
StretchKey:
  - 100,000 samples: 10 / (1 − 0.9999). With fewer, `lat_hist.hpp` prints `p99.99=n/a`, because a percentile resting on one or two samples is noise.
</details>

---

## Make a broken plan fail loudly

### CONCEPT C17  Forbid the fallback past the peak

**Why.** A prepared pool is only as good as your peak estimate. If the market gives you more orders than you planned for, the pool quietly refills from the heap. The tail comes back, and no test you wrote goes red. LPA would rather *know*. So give the pool a fixed slab of memory whose own upstream refuses: `std::pmr::null_memory_resource()` (lesson 01) throws `std::bad_alloc` on every request.

```cpp
// fragment: the pool's chunks come from a slab you sized for the peak
alignas(64) static std::byte buf[kSlabBytes];
std::pmr::monotonic_buffer_resource slab(buf, sizeof buf, std::pmr::null_memory_resource());
fixed_pool pool(4096, &slab);   // past the slab: std::bad_alloc, never the heap
```

**Gives up:** a request past the peak now *fails*. You must decide what that means: reject the order, or shed load. You also lose the heap's flexibility.

**Rule:** past the planned peak, fail loudly, never fall back quietly. Fine print: `fixed_pool`'s chunk bookkeeping `std::vector` still uses the global heap while it refills, so this guards the chunks, not every byte.

### Card M5-LPA5 · Fix it
Q: The pool's chunks must come from `buf`. Once `buf` is used up, any further refill must throw `std::bad_alloc` instead of quietly reaching the heap. Give the one expression that fills the blank.
   ```cpp
   // Fragment: fixed_pool, Order and buf come from the lab.
   std::pmr::monotonic_buffer_resource slab(buf, sizeof buf, ___);
   fixed_pool pool(256, &slab);
   std::pmr::list<Order> book(&pool);
   ```
   Answer format: one C++ expression on one line.

<details><summary>Answer</summary>
Key:

  - `std::pmr::null_memory_resource()`. Its `allocate` always throws `std::bad_alloc`, so the slab cannot fall back to anything. `new_delete_resource()` or `get_default_resource()` would silently use the heap. `nullptr` is not a resource at all: past the slab that is undefined behaviour. `new std::pmr::monotonic_buffer_resource(...)` is refused too: it would leak a heap-allocated resource. A trailing `;` is fine.
</details>

### Card M5-LPA6 · Recall
Q: Name three things the prepared-and-forbidden pool gives up, compared with the default heap.
   Answer format: one line; separate the three with semicolons.

<details><summary>Answer</summary>
Key:

  - Memory: the worst-case peak is held all day, even in quiet hours (about 80 MB for 2,000,000 40-byte nodes). `fixed_pool` returns nothing until it is destroyed.
  - Start-up time: the warm-up pays every refill and page fault before you go live.
  - Flexibility: you must know the peak and the one request size. Past the peak you get `std::bad_alloc` (or, without the guard, the tail comes back). Another size class needs its own pool.
</details>

---

## Close

**Remember:** a rare cost hitting fraction *f* of samples lives above percentile 1 − *f*; pay it before the clock starts; past the plan, fail loudly. **Tomorrow:** missed cards come back. **Next:** module 06, hot-path patterns.
