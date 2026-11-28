# Module 03 — Concurrency: Threads, Atomics, Memory Order, Coroutines

What this module adds: starting and stopping threads safely, the C++ rules
that decide whether two threads touching the same data is legal (data race,
happens-before), choosing memory orders, the C++20 coordination types, and
C++20 coroutines. It assumes module 01 (lambdas, move, RAII) and a C++20
compiler with ThreadSanitizer (`-fsanitize=thread`); it was tested with
Apple clang 21 on macOS arm64.

## Subject & scope
- **Subject:** `std::thread`, `std::jthread` and `stop_token`, `std::async`;
  data races and happens-before; `std::atomic` and the memory orders;
  compare-exchange; mutexes and spinlocks; cache lines and false sharing;
  `std::latch`, `std::barrier`, `std::atomic_ref`; coroutines
  (`promise_type`, `coroutine_handle`, awaiters, frame allocation).
- **Grain:** decide whether a given piece of two-thread code has a data
  race, fix publication with release/acquire, and explain the cost and
  lifetime rules of each tool.
- **Runnable samples:** `lab/*.cpp`; each file's third line is its build
  command (run from the tutorial root; programs go to your temp directory).

## Time cost (honest)
- **First pass:** about 151 minutes over 5 lessons (24 + 32 + 29 + 39 + 27; lesson 05 is the LPA lens on hand-off),
  counted as reading at about 200 words a minute plus about 1 minute per
  pretest, 1.5 per completion card, 2 per question and 4 per synthesis card,
  plus about 1 minute per lab program you build. 40 cards: 6 pretests,
  6 completions, 12 questions, 10 gotchas, 4 judgement cards, 2 synthesis.
- **Optional practice bank:** lesson 06, about 188 minutes in all, split into eight themed parts of
  at most 30 minutes each: `lesson-06-practice-bank-01-compare-exchange.md` (26 min),
  `lesson-06-practice-bank-02-exchange-locks.md` (30 min), `lesson-06-practice-bank-03-wait-wake.md`
  (30 min), `lesson-06-practice-bank-04-visibility.md` (28 min),
  `lesson-06-practice-bank-05-threads-cores.md` (29 min), `lesson-06-practice-bank-06-fences.md`
  (27 min), `lesson-06-practice-bank-07-cas-equality.md` (8 min),
  `lesson-06-practice-bank-08-wait-address.md` (10 min); 19 self-graded cards on measurement, research and C++ techniques and on how
  results expire. It is optional and not counted in the first pass above; its
  runnable programs are in `lab/practice/`.
- **Reviews:** the driver schedules misses for the next day and uninterrupted hits about
  2, 9, 30, 51 and 72 days after the first attempt; each sitting is capped by the driver.
- **Interruptible:** yes; progress is saved after every card.

## Learning objectives
1. Given code that starts threads, the learner can say when it calls
   `std::terminate`, rewrite it with `std::jthread` and `stop_token`, and
   predict where and when a `std::async` task runs and who waits for it.
2. Given two threads and a shared variable, the learner can decide whether
   there is a data race, using the definitions of conflict and
   happens-before, and repair a flag publication with a release store and an
   acquire load.
3. The learner can choose the weakest correct memory order for a counter and
   for a publication, judge whether an instruction comparison proves a
   whole-program ordering claim, and write a `compare_exchange_weak` retry loop.
4. Given a coordination need (short critical section, per-thread counters,
   one-time start-up, repeated phases, atomic access to an existing `int`),
   the learner can choose among `mutex`, spinlock, padding, `latch`,
   `barrier` and `atomic_ref`, and state the cost or lifetime condition.
5. The learner can read a minimal C++20 generator and awaiter, name what
   `promise_type`, `coroutine_handle` and an awaiter do, and explain when a
   coroutine frame is allocated and how to control that on a hot path.
6. LPA lens on hand-off (lesson 05): compare blocking and busy-polling
   hand-offs percentile by percentile, write a correct acquire spin wait, and
   spot oversubscribed or low-priority spinners (efficiency-core caveat).

## Tiers supported
```
tiers: [T0, T1, T2, T3]
detect:
  - model + tools -> T3   (Socratic, adaptive, published transfer items only)
  - model, no tools -> T2 (grades free text vs Key bullets)
  - python3 drive.py -> T1 (regex AcceptIf / self-report, no model)
  - else -> T0            (open lessons/*.md and self-quiz)
```
Run T1 with:
`python3 ../../drive.py lessons --progress progress.json --today $(date +%Y-%m-%d)`

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
