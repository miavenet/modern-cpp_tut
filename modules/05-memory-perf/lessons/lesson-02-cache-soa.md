# Lesson 02 — Cache Lines and Data Layout: Where Latency Lives

**Time: about 29 minutes** (reading about 12, cards about 17; running the samples is optional extra). Stop after any card; your place is saved.

**Hook:** Two threads update separate counters. Predict whether placing those counters on one cache line will change throughput.

**You will be able to:** work out how much of each cache line a loop uses, choose between "array of structs" and "struct of arrays", and pad per-thread data for the machine in front of you.

**Needs from earlier:** module 03's thread and atomic basics. Here, `fetch_add` atomically increments a counter; `memory_order_relaxed` preserves atomicity without ordering unrelated data.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### Card M5-P2 · Pretest
Q: You have 10,000 `Order` objects of 120 bytes each, and a loop that reads only the 8-byte `price` of each one to find the maximum. You move all prices into one contiguous `std::vector<double>`. Guess in one sentence: what changes for the loop, and why?
<details><summary>Answer</summary>
Key:

  - The loop touches far fewer bytes: about 80 KB of prices instead of about 1.2 MB of whole orders, because memory moves in whole cache lines and the old layout dragged the unused 112 bytes of each order along.
  - Usually that makes the scan faster, and contiguous doubles also let the compiler vectorise; how much faster is a measurement on your machine, not a rule.
</details>

---

## Cache lines and layout

### CONCEPT C4  Cache lines, hot/cold splitting, and SoA

**Why this layout exists.** The C++98 habit of putting every fact about an order in one struct is convenient until a loop reads only prices. The CPU still fetches lines holding the nearby client and venue fields. You paid to move data the loop never asked for.

**Words first.** A *cache* is a small, fast memory next to each CPU core. Data moves between main memory and cache in fixed blocks called *cache lines*. Read one byte and the whole line comes with it. Line size depends on the CPU: on the course's Apple Silicon test machine `sysctl hw.cachelinesize` prints `128`; 64 bytes is common on x86-64 (untested here). *Hot* fields are read on the fast path; *cold* fields rarely.

**The habit vs the layout.** One struct per order (an *array of structures*, AoS) puts hot and cold fields side by side:

```cpp
#include <cstdint>
#include <cstdio>
#include <vector>

struct OrderBad {               // AoS: hot and cold mixed
    std::uint64_t id;
    double price;               // hot
    std::uint64_t qty;          // hot
    char client[64];            // cold: used at end of day
    char venue[32];             // cold
};

struct OrderCold { char client[64]; char venue[32]; };
struct OrderHot {               // hot/cold split
    std::uint64_t id;
    double price;
    std::uint64_t qty;
    OrderCold* cold;            // cold fields live elsewhere
};

struct OrderBookSoA {           // SoA: one array per field
    std::vector<std::uint64_t> ids;
    std::vector<double> prices; // a price scan reads only this
    std::vector<std::uint64_t> qtys;
};

int main() {
    std::printf("OrderBad %zu bytes, OrderHot %zu bytes\n",
                sizeof(OrderBad), sizeof(OrderHot));
}
```

Predict the two sizes first.

<details><summary>Real output (arm64, Apple clang)</summary>

```text
OrderBad 120 bytes, OrderHot 32 bytes
```

Walk the price scan over 10,000 orders. AoS: each order is 120 bytes, so the loop drags about 1.2 MB through the cache to use 80 KB of prices. Hot/cold: 32-byte orders, about 320 KB. SoA: the prices are contiguous, 80 KB; one 128-byte line holds 16 of them.
</details>

**The wrong belief:** "a 32-byte struct sits in one cache line." Only if its address keeps it from crossing a line boundary; size alone does not guarantee that.

**Dense flags trap.** `std::vector<bool>` is a special case: it packs flags into words (libc++ here gives a capacity of 64 flags for a single element). `v[i]` returns a *proxy* object, not a `bool&`, so `bool* p = &v[0];` does not compile, and each access does bit arithmetic. For a byte interface, use `std::vector<char>` or `std::vector<std::uint8_t>`. For a `bool*` interface, use actual `bool` storage such as `std::unique_ptr<bool[]>`.

| Layout | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Array of structs | Each order's fields stay together | Most operations use several fields of one order | A one-field scan fetches mostly unused fields |
| Hot/cold split | Extra pointer and second storage to manage | Hot path needs a few fields; cold fields are occasional | Pointer chasing appears in the hot loop |
| Struct of arrays | Separate arrays must stay at matching indices | A loop scans one field across many orders | Each operation gathers several fields through separate arrays |

Choose the layout for the loop you actually run most; a cache profile or timed representative scan settles a close call.

**Rule:** lay data out by how loops read it: fields read together go together, and a scan of one field wants that field contiguous.

### Card M5-Q14 · Recall
Q: A scan reads one 8-byte field from each 128-byte record. With 128-byte-aligned records and cold 128-byte cache lines, compare useful bytes and fetched bytes with a contiguous array of that field. Answer in one or two sentences.
   Answer format: one line.
<details><summary>Answer</summary>
Key:

  - The record layout uses 8 of each fetched 128 bytes; the contiguous layout can use all 128. For many records, the idealized fetched-byte ratio is 16:1.
</details>

### Card M5-J2 · Choose a layout
Q: A workload changes from scanning prices to repeatedly updating several fields of one order. Does the previous SoA choice still follow? What would you test?
   Answer format: one line.
<details><summary>Answer</summary>
Key:

  - No. Reconsider AoS or a hot-field grouping because the access pattern changed; benchmark the representative update.
</details>


### Card M5-G3 · Spot the trap
Q: You store 1 million flags in `std::vector<bool>` "for cache efficiency" and plan to hand `&flags[0]` to a C function taking `bool*`. Name the trap in one sentence.
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - `vector<bool>` returns proxy references, so `&flags[0]` cannot supply the required `bool*`. Use actual `bool` storage, such as `std::unique_ptr<bool[]>`, for this interface; byte containers fit interfaces that accept bytes.
</details>

---

## False sharing

### CONCEPT C5  False sharing, `alignas`, and the interference-size constant

**The queue that was as slow as a disk.** The LMAX team was building a financial exchange. Profiling their early pipeline implementations showed that queueing events between stages dominated the cost. In their tests, queue latency was comparable with disk I/O. They called it "dramatically slow".

Part of the reason hid in memory layout. Even when a queue keeps its head and tail in separate variables, guarded by separate locks or atomics, those variables can occupy the same cache line. The producer writes one, the consumer writes the other, and the line can bounce between cores.

The team calls this way of thinking "mechanical sympathy": design for how the hardware really works. Their rule for this case is to keep independently written values on separate cache lines. That is why padding a pair of counters can matter even when no thread ever touches the other's counter.

**Words first.** A *data race* involves conflicting accesses from different threads, at least one non-atomic, with neither ordered before the other by happens-before; it is undefined behaviour. *False sharing* is a performance problem: different locations share a cache line, and writes invalidate another core's cached copy.

**C++98 way vs modern way.**

```cpp
// fragment (needs <atomic>): C++98 pads by hand; this assumes 8-byte long
struct Stats98 { long a; char pad[120]; long b; };   // not a portable line boundary

// C++11: alignas puts each member at a multiple of the given alignment
struct Stats { alignas(128) std::atomic<long> a; alignas(128) std::atomic<long> b; };
```

**See it yourself.** Compile with `clang++ -std=c++20 -Wall -Wextra -O2 fs.cpp && ./a.out` and compare your own times:

```cpp
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

template <std::size_t Align>
struct alignas(128) Counters {
    alignas(Align) std::atomic<long> a{0};
    alignas(Align) std::atomic<long> b{0};
};

template <std::size_t Align>
long long run() {
    Counters<Align> c;
    auto start = std::chrono::steady_clock::now();
    std::thread t1([&] { for (int i = 0; i < 50'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
    std::thread t2([&] { for (int i = 0; i < 50'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    t1.join(); t2.join();
    auto d = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

int main() {
    std::printf("8-byte apart:   %lld ms\n", run<8>());
    std::printf("64-byte apart:  %lld ms\n", run<64>());
    std::printf("128-byte apart: %lld ms\n", run<128>());
}
```

On a 128-byte-line target, aligning the whole object to 128 bytes makes the 8-byte and 64-byte pairs share a line; the 128-byte pair occupies separate lines. Timings still depend on scheduling and the workload.

**Packed flags are the exception.** For standard containers, writes to *different* elements do not race. `std::vector<bool>` is excluded: its flags share words, so the standard says `y[0] = true` can race with `y[1] = true`.

**Which number to pad to?** C++17 added `std::hardware_destructive_interference_size` in `<new>`: the implementation's minimum recommended separation for concurrently accessed objects. It is a compile-time constant, not a measurement of your CPU. If it recommends 256 while your target has 128-byte lines, that may spend extra space; measure before reducing it.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `alignas(64)` | At least 64-byte alignment; target line may be wider | A measured 64-byte-line target | Two padded counters still contend |
| Target line alignment | Spends one target line per counter | A known deployment target | Target changes and the old padding remains |
| `hardware_destructive_interference_size` | Compiler-chosen separation; may use more space | Portable source built for a known target | Memory footprint grows without a contention win |

Measure contention on the deployment target, then keep the smallest spacing that actually separates the writers.

**Rule:** start by separating independently accessed hot fields by the target's cache-line size, then measure. Fine print: reader/writer false sharing and cache-line boundaries matter too.


### Card M5-C2 · Fix it
Q: Fill in the blanks so each per-thread counter starts at a 64-byte boundary (the line size this exercise assumes). Answer on one line: the two blanks in order, separated by commas.
   ```cpp
   struct WorkerStats {
       ___(64) std::atomic<std::uint64_t> thread1_processed;
       ___(64) std::atomic<std::uint64_t> thread2_processed;
   };
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - (1) `alignas`
  - (2) `alignas`
</details>

### Card M5-Q3 · Recall
Q: (a) In one sentence, what is false sharing? (b) In one sentence, what does `alignas(64)` on each of two counters guarantee, and what does it not guarantee on the course's test machine?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) Different variables share a cache line; a write can invalidate another core's cached copy even when that core only reads its own variable. The result remains correct but contention can cost speed.
  - (b) Each counter starts at a multiple of 64 bytes, so they are at least 64 bytes apart; with 128-byte lines (as on the test machine) they can still share a line.
</details>

### Card M5-Q4 · Apply
Q: Only the strategy thread writes these three counters; a monitoring thread reads them once a second. An AI assistant padded each one to the test machine's 128-byte line, saying this "removes false sharing" (fragment, needs `<atomic>`). Is the padding right here? What would you check first, and what would change your verdict?
   ```cpp
   struct StrategyStats {                        // written by the strategy thread only
       alignas(128) std::atomic<long> fills{0};
       alignas(128) std::atomic<long> rejects{0};
       alignas(128) std::atomic<long> cancels{0};
   };
   static_assert(sizeof(StrategyStats) == 384);  // three lines instead of one
   ```
   Answer format: one line; separate (a), (b), and (c) with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) The padding is legal, but this workload does not justify it without measurement. One writer and an occasional reader usually make compact counters a sensible starting point; padding cannot remove the monitoring thread's genuine sharing of the counters.
  - (b) Check which threads read and write each field, how frequently, and which fields share a cache line. False sharing can involve a writer and a reader of different fields.
  - (c) Separate independently accessed fields if measurements show harmful contention. Compare against the compact layout using the actual monitoring frequency.
</details>

### Card M5-G4 · Spot the trap
Q: Your compiler recommends 256-byte separation, but the deployment CPU reports 128-byte cache lines. Which value would you start with, what space cost would you expect, and what evidence would justify reducing it?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - Start with the implementation's 256-byte recommendation. That can use about twice the space per counter as 128-byte spacing; reduce it only after checking the actual layout and measuring contention on the deployment target.
</details>

### Card M5-G11 · Spot the trap
Q: Your risk engine keeps halt flags in `std::vector<bool> halted(kSymbols)`. Thread A writes only NYSE indices, thread B only Nasdaq indices; the index sets never overlap. (a) Is this a data race? (b) Would `std::vector<char>` change the answer? One sentence each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) Yes, it can be: packed flags share words, and the standard's rule that writes to different elements do not race explicitly excludes `vector<bool>` (`y[0] = true` can race with `y[1] = true`). Undefined behaviour.
  - (b) Yes: each `char` is its own memory location, so disjoint indices do not race; nearby hot flags may still cause false sharing, which costs speed, not correctness.
</details>

---

## Close

**Remember:** memory moves in whole lines, so lay out data by how loops read it; separate data written by different threads by the target's line size, checked, not assumed. **Tomorrow:** missed cards come back. **Next:** lesson 03, moving work to compile time.
