# Module 06 — Low Latency Patterns: Lock-Free Hand-Over, Hot Paths, Measurement, Prefetch

**What this module adds:** the patterns that sit on a trading system's
critical path. An SPSC ring with lock-free bookkeeping that you
finish and check with ThreadSanitizer, the rules a hot path follows and the
mechanism behind each, latency measurement that does not hide stalls, and
software prefetch measured rather than assumed.

**What it assumes:** module 03 (threads, atomics, `memory_order`) and
module 05 (cache lines). Everything was tested with Apple clang 21 and
libc++ on macOS arm64. Where a topic is Linux-only (transparent huge pages,
kernel bypass, `mlockall`) or x86-only (`rdtsc`, `lfence`), the lesson says
so next to it and gives the arm64/macOS counterpart where one exists.

## Subject & scope
- **Subject:** SPSC ring buffer with release/acquire counters; why SPSC fails
  with several producers; a bounded MPMC ring and its progress limits; ABA;
  hot-path contract (allocation, system calls, locks, `throw`, indirect calls);
  first-touch page faults; pre-encoded messages; hot/cold logging; transparent
  huge pages (Linux); coordinated omission; tick counters on x86-64 and arm64;
  hardware timestamps; kernel bypass and polling (Linux, DPDK); NUMA placement;
  `[[likely]]`/`[[unlikely]]` and profile-guided optimisation;
  `__builtin_prefetch`, prefetch distance, dependency chains.
- **Out of scope:** thread pinning and `isolcpus` (named in earlier versions
  without a card or a test on this platform).

## Time cost (honest)
- **First pass:** about 183 minutes over 5 lessons (40 + 30 + 43 + 40 + 30),
  39 cards (5 pretests, 5 completions, 29 recall/apply/gotcha/synthesis
  including the SPSC lab check, and the capstone lab).
  That includes the SPSC lab (about 10 minutes) and running the measurement
  programs (about 10 minutes in total).
- **Reviews:** cards come back on the driver's schedule (see the README); a
  missed card returns the next day.
- **Interruptible:** stop after any card; the driver resumes where you left off.

## Learning objectives
1. Given the SPSC ring lab, the learner can choose the memory order of each
   counter operation, explain the release/acquire pairing, and confirm the
   repair with ThreadSanitizer and the lab's value tests.
2. Given a hot-path function, the learner can name operations whose duration
   the code does not bound and move preparation off the path.
3. Given a latency claim, the learner can identify coordinated omission, read
   a tick counter, and check whether a branch hint changed generated code.
4. Given a memory-bound loop, the learner can write a prefetch call and decide
   by measurement whether it helped.
5. Given a latency histogram from this course's harness, the learner can say
   whether it has enough samples for its tail percentiles, account for the
   clock floor, and say what a median or a short run hides (lesson 03, C11).
6. Capstone (lesson 05): drive a hand-off test open-loop from intended send
   times, read per-stage histograms against a stated budget, and name the
   stage that owns the tail and the next fix with its cost.

## Ways to study

tiers: [T0, T1, T2, T3]

The offline driver runs the cards and their checks; the lessons also work as
plain Markdown with folded answers. An optional AI tutor can use PROTOCOL.md.

## Labs and runnable samples

The `lab/` folder contains the SPSC ring, bounded MPMC example, hot-path
examples, measurement programs and prefetch examples.
`lab/measure/lat_hist.hpp` is the shared latency histogram every LPA lab uses
(with `clock_tail.cpp`, lesson 03). The capstone is `lab/budget/{starter,solution}/tail_budget.cpp`
(lesson 05; work copy in `lab/work/`).

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
