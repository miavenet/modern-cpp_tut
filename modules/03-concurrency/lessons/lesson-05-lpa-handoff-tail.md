# Lesson 05 — LPA Lens: Spin or Sleep, Measured at the Tail

**Time: about 27 minutes** (reading about 8, running one program about 3, cards about 16). Stop after any card; the driver resumes there.

**Hook:** handing a message to another thread by spinning was 46 times faster than a condition variable at the median on the course Mac. At p99.99 it was only about 3.5 times faster, and the run-to-run ranges overlapped. And when macOS put the spinning thread on an efficiency core, one run's slowest hand-off took seven *seconds*.

**You will be able to:** say what a busy-poll buys and costs at each percentile, write a spin wait that is correct under the memory model, and spot the setups where spinning is the wrong answer.

**Needs from earlier:** lesson 02 (acquire/release) and lesson 03 (concept C7, mutex vs spinlock).

## Pretest — commit your guess FIRST

### Card M3-LPA1 · Pretest
Q: Two threads play ping-pong 200,000 times, one round every 20 µs. Version A blocks on a `std::condition_variable`; version B spins on a `std::atomic<int>`. Guess the ratio A ÷ B at the median (p50) and at p99.99.
<details><summary>Answer</summary>
Key:

  - p50: about 46x (5,759 ns against 125 ns, median of 7 runs on the course Mac).
  - p99.99: only about 3.5x (about 1.2 ms against 0.35 ms), and the run-to-run ranges overlapped. The far tail of *both* was set by the OS taking a core away, which spinning does not prevent on a shared Mac.
</details>

---

## The tail and its mechanism

### CONCEPT C14  LPA lens: busy-poll the hand-off, and know what the OS still owns

The *Latency Prioritized Approach* (LPA) judges a path by its far tail (p99.99), not the mean. Every LPA choice answers three questions:

- **Default does / LPA does:** the default waiting thread *blocks* (mutex plus condition variable): the kernel parks it and wakes it when notified. LPA keeps the waiting thread running and *polls* an atomic until the value changes.
- **Why (mechanism):** a blocked thread pays a wake-up: the notifier makes a system call, the kernel picks the thread, finds it a core, and restores its state. That is microseconds, and longer when the scheduler is busy. A spinning thread is already running on a core with the cache line in reach, so it sees the store within about the time the line takes to move between cores.
- **Gives up:** a whole core per spinner, burning power even when idle (the lab's counter: about 2 cores busy instead of 1); fairness to other threads; and it only works when the spinner *keeps* its core. On a machine with more busy threads than cores, or on a slow core, the spinner itself becomes the stall.

**What we measured.** `lab/handoff_tail.cpp` (build line on its third line). Course Mac: Apple M1 Max (8 performance + 2 efficiency cores), macOS 27, Apple clang 21, `-O2`, no core pinning (macOS offers none), shared machine (1-minute load average 9.9 to 11.5 during the runs, so other programs competed for cores). 200,000 round trips per row; each sample is a full round trip. Median [lowest–highest] of 7 runs.

| Wait | p50 | p99 | p99.9 | p99.99 | cores busy |
|---|---|---|---|---|---|
| condition variable | 5,759 ns | 38.9 µs | 311 µs | 1.25 ms [0.14–3.7 ms] | 0.94 |
| `atomic::wait` (C++20) | 6,783 ns | 43.0 µs | 205 µs | 0.74 ms [0.06–1.9 ms] | 1.17 |
| spin | 125 ns | 463 ns | 16.9 µs | 0.35 ms [0.03–3.0 ms] | 1.98 |
| spin, responder on an E-core | 423 ns | 49.2 µs | 786 µs | 8.1 ms [3.1–43 ms] | 1.67 |

"Cores busy" is CPU time ÷ wall time for the whole program; the pinger busy-waits through the 20 µs gaps, so 1.0 is the pinger alone.

What to read off it:

- *Spinning wins big from p50 to p99.9* (46x, 84x and 18x). The wake-up is gone.
- *At p99.99 the win shrinks to about 3.5x and the ranges overlap.* Those samples are the OS preempting a thread, and that happens to spinners too. On Linux you would isolate cores for the spinner; macOS has no way to do that.
- *`atomic::wait` was no faster than the condition variable here.* With a 20 µs gap it really sleeps (1.17 cores busy). In a variant without gaps (5 runs) it never slept: p50 125 ns and p99.99 335 ns, as fast as the spin, with 2 cores busy. libc++ polls for a short while before it sleeps. Measure your own rate before choosing it.
- *The efficiency-core caveat.* The E-core row asked macOS for background quality of service (QoS) for the responder. macOS steers background-QoS threads toward the two efficiency cores and gives them low priority (we can see the effect, not the placement: macOS reports no core number). Its median more than tripled, its p99.99 was about 23x the plain spin, and one run's max was 7.1 s.

Trust the ratios more than the exact nanoseconds; the far tail moved by 10x or more between runs on a busy machine.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Mutex + condition variable | Microsecond wake-up; costs no core while idle | Rare events, many waiting threads | A latency path paying 5 µs per hand-off |
| `atomic::wait` | Polls briefly, then sleeps (libc++) | Sporadic hand-offs where a core is too dear | Assuming it never sleeps |
| Busy-poll | ~100 ns hand-off; one core per spinner, always | Few threads, each with a core of its own | More spinners than cores; spinner at low QoS |

**Rule:** spin on a hand-off only when the spinner owns a fast core; otherwise block. Fine print: spinning removes the wake-up, not the OS; the far tail still needs a quiet machine.

### Card M3-LPA2 · Recall
Q: Name the cost a condition-variable hand-off pays that a spinning one does not, and the cost the spinning one pays instead.
   Answer format: one line; separate the two with a semicolon.

<details><summary>Answer</summary>
Key:

  - The wake-up: a system call to notify, then the kernel scheduling the parked thread onto a core and restoring it (about 5.8 µs at p50 here, far more in the tail).
  - A core: the spinner runs at 100% all the time, even with nothing to do (about 2 cores busy instead of 1 in the lab), and it can still be preempted.
</details>

### Card M3-LPA6 · Fix it
Q: Worked first. The pinger publishes with `payload = 42; turn.store(1, std::memory_order_release);`. Now finish the responder's side, one word fills the blank; the loop must pair with that release store so the `payload` write *happens before* the read.
   ```cpp
   // Fragment: turn is std::atomic<int>; payload is a plain int.
   while (turn.load(std::memory_order____) != 1) {}
   use(payload);
   ```
   Answer format: one memory-order name.

<details><summary>Answer</summary>
Key:

  - `acquire`. The acquire load that reads the release store's value synchronizes with it, so the payload write happens before the read. The next card asks you to write the whole statement yourself.
  - `relaxed` ends the loop but orders nothing (ThreadSanitizer reports a race on `payload`); `consume` orders only reads that depend on `turn`, and `payload` does not.
</details>

### Card M3-LPA3 · Build and check
Q: Write the responder's wait: one statement that busy-polls until `turn` (a `std::atomic<int>`) becomes 1, and never sleeps. After it, the responder reads a plain `int payload` that the pinger wrote *before* `turn.store(1, std::memory_order_release)`.
   Answer format: one C++ statement on one line.
   The check runs 5,000 hand-offs under ThreadSanitizer and requires every payload in order with no data race.

<details><summary>Answer</summary>
Key:

  - `while (turn.load(std::memory_order_acquire) != 1) {}`. The acquire load pairs with the pinger's release store, so the payload write *happens before* the read.
  - With `memory_order_relaxed` the loop still ends, but nothing orders the payload: ThreadSanitizer reports a data race. `memory_order_consume` is refused: it orders only reads that depend on the loaded value, and `payload` does not. `turn.wait(0)`, `sleep_for` or `yield` are refused because they can give up the core, and that is what this hand-off was built to avoid. Comments, a trailing `;` and `for (; ...; ) {}` are fine. **Not accepted:** a relaxed spin followed by an acquire fence (`std::atomic_thread_fence`). It is valid C++, but ThreadSanitizer does not model fences, so this check cannot grade it; use the acquire load as the loop condition. Statements that `return`, `break`, declare functions or call sanitizer hooks are refused too.
</details>

### Card M3-LPA4 · Spot the trap
Q: A colleague's market-data service on a 10-core Mac (8 performance, 2 efficiency) runs 12 threads that each busy-poll their own queue, and starts the whole service with `nice -n 10` "so it does not slow the desktop". Its p99.99 is worse than the old blocking version. Name the two mistakes.
   Answer format: one line; separate the two with a semicolon.

<details><summary>Answer</summary>
Key:

  - More spinners than cores: 12 always-running threads on 10 cores means some spinner is always waiting for a core, and while it waits nothing polls its queue. The OS time-slices them, so a hand-off can wait a whole scheduling quantum.
  - `nice` lowers the threads' priority, so the scheduler gives their cores away first; and background QoS, the other way to "be polite" on macOS, steers threads toward the efficiency cores. Both are where the lab's spin was slowest (low-QoS responder: p99.99 about 23x worse, one 7 s max). Keep spinners few, at normal priority, on cores they own; block everything else.
</details>

### Card M3-LPA5 · Recall
Q: Name three things a busy-polling hand-off gives up compared with blocking.
   Answer format: one line; separate the three with semicolons.

<details><summary>Answer</summary>
Key:

  - A whole core per spinner, all day, idle or not (power and heat with it).
  - Room for other work: other threads, including your own, get fewer cores; with more spinners than cores the tail gets worse, not better.
  - Protection from the OS: if the spinner is preempted or placed on a slow core, nobody is listening; blocking at least gets woken when it matters.
</details>

---

## Close

**Remember:** spinning removes the wake-up (46x at p50 here), not the OS (only 3.5x at p99.99); one spinner per fast core, normal priority, acquire load. **Tomorrow:** missed cards come back. **Next:** module 04, the modern standard library.
