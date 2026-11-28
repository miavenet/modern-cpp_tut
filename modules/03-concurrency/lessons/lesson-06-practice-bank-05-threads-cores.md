# Lesson 06 — Practice Bank, part 5 of 8 (optional): Many Threads at Once

**Time: about 29 minutes (estimate)** for 3 cards (6 + 15 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: wake-ups a waiter misses, contention on one atomic, and how many CPUs the process really has.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB4 · Spot the trap
Q: A status word goes BUSY -> IDLE -> BUSY very quickly. A monitor thread does `status.wait(BUSY)` to "wake whenever it becomes IDLE". Sometimes the monitor sleeps through an IDLE period. Is this a library bug? What does the standard promise, and how do you fix the design?
   Answer format: bug or not, the rule, and one fix.
<details><summary>Answer</summary>
Key:

  - Not a bug. `wait(old)` loads the value, returns if it differs from `old`, otherwise blocks until notified (or spuriously), then repeats. If the waiter re-checks after the value is back at BUSY, it keeps blocking.
  - The standard says so: programs "are not guaranteed to observe transient atomic values, an issue known as the A-B-A problem, resulting in continued blocking if a condition is only temporarily met."
  - Fix: make every change visible, for example a counter or sequence number that only increases (wait on the old count), or pack state and a generation into one word.
  - Holds for / re-check: the quoted text is a note in [atomics.wait]; it follows from the normative Effects of `wait` ([atomics.types.operations]: load, return if different, else block, repeat), so it holds on all implementations, C++20 onward.
</details>

### Card M3-PB11 · Apply
Q: `rmw_scale.cpp` runs T threads (1, 2, 4, 8). Each does 5 million operations in one of three modes: `shared` (all threads `fetch_add` one atomic), `private` (each thread `fetch_add`s its own atomic on its own 128-byte-aligned line), `load` (all threads only load the shared atomic; nobody writes it). The program prints elapsed wall time divided by the 5 million operations of one thread, not divided by 5 million times T. Before running, predict for each mode: does that figure grow as T grows, and which mode grows most steeply? Check the listing first (find the worker loop of each mode), then run every cell three times:
   ```sh
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/rmw_scale.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/rmw_scale.cpp -o "${TMPDIR:-/tmp}/rmw_scale" && for m in shared private load; do for t in 1 2 4 8; do "${TMPDIR:-/tmp}/rmw_scale" $m $t; done; done
   ```
   Answer format: the direction per mode, what it says about atomics, then the instruction in each worker loop.
<details><summary>Answer</summary>
Key:

  - Direction, 3 of 3 runs: `shared` rises at every step from T=1 to T=8 and rises far more steeply than `private` or `load`. `private` and `load` also drift upward a little in some runs, so do not expect flat curves; grade only the comparison.
  - So the cost is not "atomic instructions are slow": the contended cost comes from many cores writing one line. A read-only shared line scales much better: every core can keep its own copy.
  - Listing check: the `shared` and `private` worker loops contain `ldadd` (atomic read-modify-write); the `load` worker loop is `ldapr` here (acquire load; a target may use `ldar` instead). The `ldar` instructions in `main` are unrelated (they read the final totals).
  - Holds for / re-check: direction only, one machine (M1 Max, Apple clang 21, `-O2`), 3 process runs; no numbers are part of the key (one machine, varies with load). Threads are not pinned (macOS cannot pin), so no claim is made about which physical cores ran them. "Bouncing" means ownership of the line moves between cores, not a transfer on every write. The slope will differ on other chips; re-check by rerunning the 3x4 grid on the target.
</details>

### Card M3-PB13 · Spot the trap
Q: A thread pool sizes itself with `std::thread::hardware_concurrency()`. You restrict the process to one CPU with `taskset -c 0` and expect it to start one worker. Predict what `hardware_concurrency()`, `sysconf(_SC_NPROCESSORS_ONLN)` and the affinity mask (`sched_getaffinity`) each report. Run on Linux with and without `taskset -c 0`:
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/pool_size.cpp -o "${TMPDIR:-/tmp}/pool_size" && "${TMPDIR:-/tmp}/pool_size" && taskset -c 0 "${TMPDIR:-/tmp}/pool_size"
   ```
   Answer format: three numbers without `taskset`, three with it, then the habit.
<details><summary>Answer</summary>
Key:

  - Without `taskset`: `hardware_concurrency()` = 10, `sysconf(_SC_NPROCESSORS_ONLN)` = 10, affinity mask = 10 (GCC 13.3 and clang 18.1.3, both libstdc++).
  - Under `taskset -c 0`: `hardware_concurrency()` = 10, `sysconf(_SC_NPROCESSORS_ONLN)` = 10, affinity mask = 1 (same on both compilers).
  - So `hardware_concurrency()` followed the online CPU count (the same value as `sysconf`), not the affinity mask (1): a pool sized from it starts 10 workers that all share one CPU. Which source libstdc++ reads was not looked up; the observed values match the online-CPU count. Only `sched_getaffinity` saw the restriction.
  - The standard only promises "the number of hardware thread contexts" and calls it "a hint".
  - Habit: when you change isolation or affinity, print what every sizing call returns and log it with the results.
  - Holds for / re-check: Linux, glibc and libstdc++ 13 in an Ubuntu 24.04 aarch64 container. Other C libraries, container CPU limits (quotas instead of masks), and macOS (no affinity masks) can behave differently; re-run the probe.
</details>

---

## Close

Next: part 6, `lesson-06-practice-bank-06-fences.md` (about 27 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
