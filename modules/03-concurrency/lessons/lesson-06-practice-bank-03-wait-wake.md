# Lesson 06 — Practice Bank, part 3 of 8 (optional): Waiting and Waking, Measured

**Time: about 30 minutes (estimate)** for 3 cards (20 + 6 + 4 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: atomic wait against a condition variable, what the library really does, and checking SMT before tuning.

Hook: Two waiting primitives face three notification gaps; predict their CPU costs before measuring.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB3 · Apply
Q: A feed thread rings a "doorbell" every *gap* microseconds; a worker thread waits for each ring. It is built two ways: with `std::atomic<int>::wait` / `notify_one`, and with `std::mutex` + `std::condition_variable`. The worker measures its own CPU time (`clock_gettime(CLOCK_THREAD_CPUTIME_ID)`) per ring, its wake-up delay (ring time to wake time), and counts how many times it woke. Before running, predict the worker's CPU per ring for each primitive at gaps of 2, 20 and 200 us. Then build it and run the three gaps (arguments: primitive, gap in us, rings; each gap runs for about 0.3-2 s):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/doorbell.cpp -o "${TMPDIR:-/tmp}/doorbell"
   "${TMPDIR:-/tmp}/doorbell" atomic 2 166666 && "${TMPDIR:-/tmp}/doorbell" cv 2 166666
   "${TMPDIR:-/tmp}/doorbell" atomic 20 66666 && "${TMPDIR:-/tmp}/doorbell" cv 20 66666
   "${TMPDIR:-/tmp}/doorbell" atomic 200 9523 && "${TMPDIR:-/tmp}/doorbell" cv 200 9523
   ```
   Answer format: how the two primitives' CPU per ring compare at a short and at a long gap, plus one line on what to report.
<details><summary>Answer</summary>
Key:

  - Counts first (each line prints `wakes=`). At 2 us the atomic waiter woke for nearly every one of the 166,666 rings in every round; cv woke for under half of them: it handled several rings per wake. At 20 us and 200 us both woke for most rings (on this Mac over 90% in every round; on the Linux virtual machine in the Holds-for line, 69-85% at 20 us). Wake counts depend on load. Compare per-ring numbers only when the wake counts are close.
  - 2 us gap: the atomic worker's CPU per ring was the whole gap (the line's "% of the gap" near 100) in every round, so it never slept. It is fast because it burns a whole core. cv used well under half of the gap.
  - 20 us and 200 us gaps: the direction is the graded result: the atomic worker used more CPU per ring than cv in every round recorded here. How much more, and the microseconds themselves, change with load and machine and are not part of the Key.
  - Grading: a different ratio, or a well-supported inconclusive comparison (for example, wake counts too far apart to compare per ring, stated as such), does not lower the grade.
  - Wake delay: this key makes no claim about which primitive woke sooner. Across the rounds recorded here the answer changed from round to round, so one run cannot settle it. If it matters to you, run several rounds and report the spread with the load average.
  - Reading: once the gap is longer than its spin window, the atomic waiter spins, then sleeps anyway, and pays both. This fits the shipped libc++ wait code, which card M3-PB5 reads, but the run does not show the mechanism directly.
  - Lesson: report CPU cost and wake counts next to latency, and sweep the gap. A spin-then-park primitive measured only at a tiny gap shows its spin, not its park.
  - Holds for / re-check: libc++ on macOS, no pinning (the OS decides where both threads run). Another library (libstdc++ on Linux) has different spin and park code, so rerun there, and after toolchain upgrades. A libstdc++ 13 rerun (3 rounds) on Ubuntu 24.04 on arm64 in a virtual machine on a busy host (GCC 13.3 / clang 18.1.3, libstdc++/glibc) went the same direction: at 20 and 200 us the atomic worker used more CPU per ring than cv.
</details>

### Card M3-PB5 · Recall
Q: You want to know what `atomic::wait` really does on your platform before trusting a benchmark. Which source do you read, what does current libc++ do before it parks, and what does it park on (Linux and Apple)? First attempt: look it up in the SDK headers under `$(xcrun --show-sdk-path)/usr/include/c++/v1/__atomic/` (and `__thread/poll_with_backoff.h`); on reviews, recall it.
   Answer format: the source, the pre-park behaviour, and the park call for each platform.
<details><summary>Answer</summary>
Key:

  - Read the standard library's own source, and preferably the headers that ship with your toolchain, not a second-hand write-up. For libc++: `__thread/poll_with_backoff.h`, `__atomic/atomic_sync.h`, and `src/atomic.cpp`.
  - Before parking: it polls the value 64 times (`__libcpp_polling_count`), then keeps polling, with no yield or sleep, until more than 4 us have passed (`__atomic_wait_backoff_impl`: `if (__elapsed > chrono::microseconds(4))`). Only then does it park. The same holds in both `#if` branches of the header shipped here.
  - The park uses `futex` (`FUTEX_WAIT_PRIVATE` / `FUTEX_WAKE_PRIVATE`) on Linux. On Apple it uses `os_sync_wait_on_address` / `os_sync_wake_by_address_*` when the deployment target is macOS 14.4 (or iOS/tvOS 17.4, watchOS 10.4) or later, else the private `__ulock_wait` / `__ulock_wake`.
  - The sleeping `__libcpp_timed_backoff_policy` is used only in fallback paths (a platform with no native wait, or Windows without `WaitOnAddress`), and by timed waits when the new sync ABI is unavailable (older deployment target, `atomic_sync_timed.h`); not by `atomic::wait` on Linux or Apple.
  - Then confirm with a run (card M3-PB3): CPU time shows whether the waiter spun or slept.
  - Holds for / re-check: the headers shipped with this toolchain (`_LIBCPP_VERSION 220106`, Apple clang 21); the park-side choice is from upstream `atomic.cpp` as read on 2026-11-15, and the compiled Apple library was not inspected. libstdc++ is entirely different code: GCC 13's `bits/atomic_wait.h`, read on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), checks up to 16 times: the first 12 with `pause` on x86 (`sched_yield` elsewhere), the last 4 with `sched_yield`, then parks on `futex`. Re-read the source matching your toolchain version (`$(xcrun --show-sdk-path)/usr/include/c++/v1` on a Mac).
</details>

### Card M3-PB14 · Recall
Q: You read a tuning recipe that relies on two hardware threads sharing one core (SMT, "hyper-threading"). Before you try it on a new machine, what do you check, and what does this M1 Max say?
   ```sh
   sysctl hw.physicalcpu hw.logicalcpu hw.perflevel0.physicalcpu hw.perflevel0.logicalcpu hw.perflevel1.physicalcpu hw.perflevel1.logicalcpu
   cat /sys/devices/system/cpu/cpu0/topology/thread_siblings_list; lscpu | grep -i 'thread(s) per core'   # Linux
   ```
   Answer format: what you compare, what this Mac shows, then why you re-check per box.
<details><summary>Answer</summary>
Key:

  - Compare logical CPUs with physical cores. Equal means no SMT: there is no sibling thread to use, and the recipe does not apply.
  - M1 Max: `hw.physicalcpu: 10`, `hw.logicalcpu: 10`; 8 + 2 cores in two performance levels, each with logical = physical. No SMT.
  - Linux (an aarch64 container): `thread_siblings_list` for cpu0 is `0` (only itself) and `lscpu` says `Thread(s) per core: 1`. On an SMT box the list would name two CPUs.
  - Re-check per box: SMT can be switched off in firmware or by the kernel, so a model name is not enough.
  - Holds for / re-check: checked on Apple M1 Max (macOS) and an Ubuntu 24.04 aarch64 container. The sysfs path is documented under `/sys/devices/system/cpu/cpuX/topology/` (kernel.org, admin-guide/cputopology). An SMT x86 machine was not available, so its two-CPU sibling list is not shown here.
</details>

---

## Close

Next: part 4, `lesson-06-practice-bank-04-visibility.md` (about 28 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
