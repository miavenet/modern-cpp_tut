# Lesson 05 — LPA Lens: Where a Throw Lands in the Distribution

**Time: about 22 minutes** (reading about 7, running one program about 3, cards about 12). Stop after any card; the driver resumes there.

**Hook:** one order check, a million calls, one bad order in ten thousand. The mean barely moves (1.1 ns to 1.9 ns per call). But on the course Mac one percentile jumps from 167 ns to about 5,800 ns, and you can say in advance which one.

**You will be able to:** say which percentile a rare error path reaches, explain why a throw costs microseconds, write the hot-path version that cannot throw, and say what that costs you.

**Needs from earlier:** lesson 01 (concept C3, `std::expected`). Module 05 lesson 06's rule (a cost hitting fraction *f* of samples lives above percentile 1 − *f*) helps but is repeated below.

## Pretest — commit your guess FIRST

### Card M4-LPA1 · Pretest
Q: The same order check runs 1,000,000 times, each call timed on its own. 100 of the orders are bad (0.01%). Version A throws `std::invalid_argument` on a bad order; version B returns `std::unexpected(...)`. Guess: which is the lowest of p50, p99, p99.9 and p99.99 at which A is clearly slower than B, and roughly how much slower is one throw than one good call?
<details><summary>Answer</summary>
Key:

  - p99.99. 100 bad orders in 1,000,000 are exactly the slowest 0.01%, so only p99.99 and the max can see them. p50 to p99.9 were identical for A and B in all 7 runs (0, 42 and 83 ns).
  - On the course Mac, about 5,800 ns per throw against under one 42 ns clock tick for a good call: thousands of times slower. A's p99.99 was 5,759 ns [5,759–5,887], B's 167 ns [85–167].
</details>

---

## The tail and its mechanism

### CONCEPT C15  LPA lens: keep the error path off the unwinder

The *Latency Prioritized Approach* (LPA) judges a hot path by its far tail (p99.99), not by its mean. Every LPA choice answers three questions:

- **Default does / LPA does:** the default reports a rejected order by throwing; the caller catches it. LPA returns the rejection as a value (`std::expected<int, reject>`) and branches on it.
- **Why (mechanism):** a throw allocates the exception object, runs the unwinder (it looks up unwind tables for every frame between `throw` and `catch`), and runs destructors on the way. Here the message string of `std::invalid_argument` also called `operator new` once per throw (counted: 100 heap calls for 100 throws, 0 for `expected`). None of that runs when nothing is thrown, so the cost lands only on the rare calls, which is exactly where the tail is.
- **Gives up:** every caller must check the result (a branch on every call, and `[[nodiscard]]` discipline); errors no longer jump across many frames for free; constructors and operators cannot return `expected`, so they need another design.

**What we measured.** `lab/expected_tail.cpp` (build line on its third line). Course Mac: Apple M1 Max, macOS 27, Apple clang 21, `-O2`, one thread, no core pinning, shared machine (load average about 11). 1,000,000 calls per row, each timed alone for the percentile columns. The *mean ns/call* column is a different measurement: a second pass over the same orders with one clock pair around all 1,000,000 calls (no per-call clock reads, and it does not count errors), so it is not the average of the timed samples. Median [lowest–highest] of 7 runs, ns. The error count and heap calls were identical in every run.

| Run | p50 | p99 | p99.9 | p99.99 | mean ns/call | heap calls |
|---|---|---|---|---|---|---|
| throw, 0% bad | 0 | 42 | 83 | 125 [85–167] | 1.3 [1.3–1.5] | 0 |
| expected, 0% | 0 | 42 | 83 | 125 [85–167] | 1.1 [1.1–1.2] | 0 |
| throw, 0.01% | 0 | 42 | 83 | **5,759** [5,759–5,887] | 1.9 [1.9–2.0] | 100 |
| expected, 0.01% | 0 | 42 | 83 | 167 [85–167] | 1.1 [1.1–1.3] | 0 |
| throw, 1% | 0 | **5,631** | 5,887 | 6,143 [5,887–6,527] | 59.9 [58.9–60.3] | 10,000 |
| expected, 1% | 0 | 42 | 83 | 167 [85–167] | 1.1 [1.1–1.1] | 0 |

Read it with the rule from module 05: a cost that hits a fraction *f* of samples shows only in percentiles above 1 − *f*. At 0.01%, only p99.99 sees the throws. At 1%, p99 already does. Each 0 or 42 is "under one clock tick" (the steady clock here ticks every 41.7 ns), so do not read the p50 column as a speed.

**Two honest surprises.**

- *The famous happy-path win did not show up.* "Zero-cost exceptions" means no work when nothing is thrown. At 0% bad orders, the `expected` loop was not slower; its mean was about 0.2 ns *lower* in every run. A 0.2 ns gap is code layout, not a law. Either way, the branch LPA adds cost nothing you could measure here.
- *The mean nearly hides the 0.01% case.* 1.1 to 1.9 ns per call looks like noise. The same data has a p99.99 34 times higher. A dashboard that plots the mean would never show it.

**Rule:** on the hot path, a rare error is a tail event; return it as a value, and keep `throw` for failures that end the operation anyway. Fine print: measure on your own target, because the cost of one throw depends on the unwinder and the stack depth.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `throw` | Free until thrown; microseconds plus a heap call when thrown | Start-up, config, "the session is dead" failures | Rejects counted per second in production logs |
| `std::expected` | A branch at every call site; flat cost when the error happens | Routine rejects on the hot path | Callers ignore the result, or rethrow it at once |

### Card M4-LPA2 · Recall
Q: Name the two costs a throw pays that a returned `std::expected` error does not, and the counter in the lab that exposed one of them.
   Answer format: one line; separate the costs and the counter with semicolons.

<details><summary>Answer</summary>
Key:

  - Allocating the exception object (and here, its message string) and running the unwinder to find the `catch` (with destructors on the way).
  - The counter: heap calls while timed, one per throw (100 at 0.01%, 10,000 at 1%) and 0 for `expected`.
</details>

### Card M4-LPA3 · Fix it
Q: Make the hot-path check report a bad quantity with no exception machinery at all. Give the one statement that fills the blank.
   ```cpp
   enum class reject { bad_px, bad_qty };
   std::expected<int, reject> check_qty(int qty) noexcept { if (qty <= 0) ___; return qty; }
   ```
   Answer format: one C++ statement on one line.
   The check compiles your line with `-fno-exceptions` (so any `throw` fails to build) and calls it with good and bad quantities.

<details><summary>Answer</summary>
Key:

  - `return std::unexpected(reject::bad_qty)`. The result holds the error instead of a value; the caller tests it with `if (!r)` and reads `r.error()`.
  - `throw ...` does not build under `-fno-exceptions`, and in a `noexcept` function it would call `std::terminate`. `return reject::bad_qty` does not compile (an `expected` does not take a bare error), and `return {}` returns a *value* (0), which silently accepts the bad order. A default-constructed error (`return {std::unexpect}`, `std::unexpected{reject{}}`) carries the zero enumerator, `bad_px`, not `bad_qty`.
</details>

### Card M4-LPA4 · Apply
Q: A market-data gateway rejects about 1 message in 200 (0.5%) as malformed, and today it throws. You record 1,000,000 messages. Which is the *lowest* of p50, p99, p99.9 and p99.99 where the throws show up?
   Answer format: one percentile, written like `p99`.
Stretch: the same gateway reads its config file once at start-up and throws if the file is missing. Should that throw become an `expected` too? One sentence.

<details><summary>Answer</summary>
Key:

  - p99.9. The throws are the slowest 0.5%. p99 is decided by the slowest 1%, and half of those are ordinary messages, so p99 still lands on a fast one. p99.9 is decided by the slowest 0.1%, and those are all throws.
</details>
<details><summary>Stretch answer</summary>
StretchKey:
  - No need. It runs once, before the clock that matters starts, and a missing config ends start-up anyway. LPA moves *rare costs on the timed path*; it does not ban exceptions.
</details>

### Card M4-LPA5 · Recall
Q: Name two things you give up by returning `std::expected` instead of throwing on the hot path.
   Answer format: one line; separate the two with a semicolon.

<details><summary>Answer</summary>
Key:

  - Every caller must check the result: a branch per call and discipline (`[[nodiscard]]`), and an unchecked `*r` on an error is undefined behaviour.
  - Errors no longer jump across many frames by themselves: each layer must pass them up (or use `and_then`/`or_else`). Constructors and operators cannot return one, so they need a factory or a two-step design.
</details>

---

## Close

**Remember:** a rare error path is a tail event: at fraction *f* it lives above percentile 1 − *f*. A throw costs microseconds and a heap call; a returned error costs a branch. **Tomorrow:** missed cards come back. **Next:** module 05, memory and performance.
