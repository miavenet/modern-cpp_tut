# Module 05 — Memory & Performance: Allocators, Cache, Layout

**What this module adds:** control over where memory comes from (PMR arenas
and pools), how data layout meets the cache, which work the compiler must do
for you, and how to use (and not misuse) parallel algorithms. It ends with a
hands-on lab: you finish a pool allocator and prove with a counter that it
stops requesting new chunks from its upstream resource for the tested steady workload.
Lesson 06 then puts that pool under a latency histogram (the Latency
Prioritized Approach, LPA, lens): you watch the tail, not the mean, move.

**What it assumes:** modules 01-02 (RAII, move semantics, `constexpr`
basics), pointers and `new`/`delete`, and a C++20 compiler. Everything was
tested with Apple clang and libc++ on macOS arm64; lesson 04's parallel
algorithms need `-fexperimental-library` there.

## Subject & scope
- **Subject:** `std::pmr` memory resources (`monotonic_buffer_resource`,
  `null_memory_resource`, `unsynchronized_pool_resource`, a hand-written
  pool), placement new and `std::launder`, cache lines, hot/cold splitting,
  SoA vs AoS, false sharing, `consteval`/`constexpr`, `[[likely]]`,
  `noexcept`, execution policies, chrono and string literals, digit
  separators, `std::source_location`; per-operation latency percentiles
  (p50 to p99.99, sample counts, clock floor) for the pool (lesson 06).
- **Out of scope:** variable templates, `[[deprecated]]`,
  `[[no_unique_address]]` and binary literals (earlier versions of this module
  named them without teaching or testing them).

## Time cost (honest)
- **First pass:** about 184 minutes over 6 lessons (29 + 29 + 20 + 33 + 45 + 28),
  49 cards (7 pretests, 7 completions, 33 recall/apply/gotcha/synthesis, two labs). The
  lesson 05 lab is about 20 minutes of that and the lesson 06 lab about 10. Running the lesson
  samples is optional and not counted.
- **Optional practice bank:** lesson 07, about 230 minutes in all, split into nine themed parts of
  at most 30 minutes each: `lesson-07-practice-bank-01-allocation-bytes.md` (26 min),
  `lesson-07-practice-bank-02-layout-alignment.md` (24 min),
  `lesson-07-practice-bank-03-false-sharing.md` (29 min), `lesson-07-practice-bank-04-cache-line.md`
  (27 min), `lesson-07-practice-bank-05-memory-traffic.md` (27 min),
  `lesson-07-practice-bank-06-vectorization.md` (22 min),
  `lesson-07-practice-bank-07-cheaper-arithmetic.md` (20 min),
  `lesson-07-practice-bank-08-float-sums.md` (29 min),
  `lesson-07-practice-bank-09-compiler-assumptions.md` (26 min);
  27 self-graded cards on measurement, research and C++ techniques and on how
  results expire. It is optional and not counted in the first pass above; its
  runnable programs are in `lab/practice/`.
- **Reviews:** cards come back on the driver's schedule (see the README);
  each review is one card at a time, roughly a minute or two per card.
- **Interruptible:** yes, at any card; the lab's work file stays where you
  left it.

## Learning objectives
1. Back a PMR container with a local buffer, predict from the growth sequence
   when it spills to the upstream resource, and forbid the spill with
   `null_memory_resource()` (lesson 01).
2. Use placement new safely: align `std::byte` storage, keep the returned
   pointer, and know when `std::launder` is needed (lesson 01).
3. Work out how many bytes a field scan moves under AoS, hot/cold and SoA
   layouts, and explain the `std::vector<bool>` proxy and data-race traps
   (lesson 02).
4. Explain false sharing, pad per-thread data to the target's line size, and
   compare `std::hardware_destructive_interference_size` with the target's
   reported cache-line size (lesson 02).
5. Choose `consteval` vs `constexpr`, use branch hints as hints, and predict
   what happens when an exception leaves a `noexcept` function (lesson 03).
6. Compile a parallel algorithm on this toolchain, replace a shared counter by
   a reduction, and keep parallel algorithms out of latency-critical paths
   (lesson 04).
7. Finish a fixed-block pool as a `std::pmr::memory_resource` (free list,
   refill, size/alignment class, chunk bookkeeping), run it under a
   `std::pmr::list<Order>`, and show with a counting upstream that upstream
   allocations stop after the first round (lesson 05 lab; the program prints
   PASS or TODO itself).
8. Use typed duration literals, choose an integer type wide enough for a large
   literal, and capture a logging call site with `std::source_location`
   (lesson 04).
9. LPA lens on allocation: read a latency histogram for its tail, say which
   percentile a rare cost reaches, move the pool's refills out of the timed
   path (zero heap calls while timed), make a missed peak fail loudly with
   `null_memory_resource()`, and name what that gives up (lesson 06 lab).

## Tiers supported
```
tiers: [T0, T1, T2, T3]
detect:
  - model + tools -> T3   (Socratic, adaptive, published transfer items only)
  - model, no tools -> T2 (grades free text vs Key bullets)
  - python3 drive.py -> T1 (checks for fill-in cards, self-grade against the Key otherwise)
  - else -> T0            (open lessons/*.md and self-quiz)
```
Run T1 with:
`python3 ../../drive.py lessons --progress progress.json --today $(date +%Y-%m-%d)`

## Lab
`lab/starter/pool_lab.cpp` (starter, compiles and prints `RESULT: TODO`),
`lab/solution/pool_lab.cpp` (reference, prints `RESULT: PASS`). Work in a copy
under `lab/work/`; lesson 05 gives the exact commands and expected output.
Lesson 06: `lab/starter/pool_tail.cpp` (prints `RESULT: TODO`) and
`lab/solution/pool_tail.cpp`; it includes your `lab/work/pool_lab.cpp` and the
shared histogram `../06-hft-patterns/lab/measure/lat_hist.hpp`.

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
