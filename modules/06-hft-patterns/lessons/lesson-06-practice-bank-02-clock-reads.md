# Lesson 06 — Practice Bank, part 2 of 12 (optional): What One Clock Read Costs

**Time: about 26 minutes (estimate)** for 3 cards (10 + 8 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: the cost of a clock read, and the smallest step a clock really takes.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB5 · Apply
Q: You want the cost of one clock read. You read the clock twice, back to back, and record the difference, a million times. Before running, predict the median difference for `std::chrono::steady_clock` on an Apple Silicon Mac. Then build and run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_hist.cpp -o "${TMPDIR:-/tmp}/clock_hist" && "${TMPDIR:-/tmp}/clock_hist"
   ```
   What does the median tell you, and what should you do instead?
   Answer format: the median, what it measures, and what to do instead.
<details><summary>Answer</summary>
Key:

  - Usually 0 on a quiet machine. Under heavy load it can be one tick (41 ns). Either way the median is a whole number of ticks: it shows the clock's resolution, not the cost of a read. (One machine, varies with load.)
  - The smallest non-zero step is 41 ns: the counter runs at 24 MHz (`cntfrq_el0 = 24000000`), so one tick is about 41.7 ns. The timebase ratio 125/3 ns per tick says the same.
  - A read that costs less than one tick cannot be timed one call at a time. Time a loop of many reads and divide, or use a finer counter (e.g. a cycle counter, where one exists and is allowed).
  - Every percentile is a whole number of ticks (in ns, e.g. 0, 41/42, 83/84, 125, 166/167 after rounding). Check the clock's resolution before reading anything into a percentile.
  - Holds for / re-check: this Mac and macOS release; which tick the median lands on depends on machine load, and other counters have other rates. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc) the smallest step was also 41 ns (24 MHz counter; measured with a Linux port of clock_hist.cpp that is not shipped). `clock_hist.cpp` builds on macOS only (it includes `<mach/mach_time.h>`); on another OS, replace its `mach_absolute_time` and `CLOCK_UPTIME_RAW` reads with that OS's clocks. It also reads the counter frequency with `mrs cntfrq_el0`, an AArch64-only instruction; replace that line on x86-64. Re-check: print the counter frequency and the smallest non-zero difference before trusting any per-call number.
</details>

### Card M6-PB11 · Spot the trap
Q: In code review a teammate writes: "`static_assert(std::is_same_v<std::chrono::steady_clock::period, std::nano>)` passes, so our timestamps are accurate to 1 ns on every box we deploy to." What kind of property is `period`: when is it fixed, and by whom? What is wrong with the inference, and what one run on the target refutes it? `clock_step.cpp` prints the declared period and the smallest non-zero step seen between back-to-back reads. Predict, then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_step.cpp -o "${TMPDIR:-/tmp}/clock_step" && "${TMPDIR:-/tmp}/clock_step"
   ```
   Answer format: what `period` is, why the `static_assert` proves nothing about accuracy, then the refuting run.
<details><summary>Answer</summary>
Key:

  - `period` is a compile-time `std::ratio` member of the clock type: the unit in which the clock's `rep` counts. The standard calls it "the tick period of the clock" ([time.clock.req], C++23 draft N4950), but it is part of the type: the library fixes it when it is written, so a compiled binary carries the same value to every machine it runs on. Both libc++ and libstdc++ define `steady_clock::duration` as `nanoseconds` on every target (checked here: Apple clang 21 / libc++, and GCC 13.3 and clang 18 with libstdc++ on aarch64 Linux).
  - So the `static_assert` checks the library's unit, not the hardware. A compile-time check cannot see which counter the program will later read, or how often it advances.
  - The refuting run: on this M1 Max the declared period is 1/1000000000 s but the smallest observed step is 41 ns, one tick of the 24 MHz counter (`cntfrq_el0`; `mach_timebase_info` 125/3 ns), in every run. Report the measured step, not the unit, next to short timings.
  - Holds for / re-check: Apple M1 Max hardware counter; other arm64 parts and x86 TSC-based clocks step differently, while the library unit stays the same. Re-run the probe on each target machine and OS.
</details>

### Card M6-PB12 · Apply
Q: After card M6-PB11, the teammate concedes that `period` is only the library's unit and says: "Then ask the OS: `clock_getres` gives the real resolution." `tick_step.cpp` reads the clock twice back to back, 200,000 times, and prints how many pairs gave a zero difference, the smallest nonzero differences, what `clock_getres(CLOCK_MONOTONIC)` reports, and on arm64 the generic timer frequency register `cntfrq_el0`. Does `clock_getres` give you the step to trust? Predict what it reports on macOS and in a Linux VM on the same Mac, then run it. Which of the four numbers do you trust, and what do you do with a 10 ns event?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/tick_step.cpp -o "${TMPDIR:-/tmp}/tick_step" && "${TMPDIR:-/tmp}/tick_step"
   ```
   Answer format: what `clock_getres` says on each OS against the observed step, which number you trust and why, then the action.
<details><summary>Answer</summary>
Key:

  - The observed steps were 41 and 42 ns (then 83-84 ns), in 3 of 3 runs on the Mac and 3 of 3 in a Linux VM on it. Many pairs read 0 on both; that share changes from run to run with load (here from under half to most) and is not part of the Key.
  - `clock_getres(CLOCK_MONOTONIC)` is wrong in both directions on the same hardware: 1000 ns on macOS (coarser than the observed step) and 1 ns in the Linux VM (finer than it), while the observed step is about 41.67 ns on both. So the OS-reported resolution is a third number, separate from the library unit and the observed step; it is not a measurement of the clock you read.
  - Trust the observed step: it matches one tick of the 24 MHz counter (`cntfrq_el0` = 24,000,000 Hz, 41.67 ns). That consistency does not by itself prove which timer the C++ library reads.
  - The smallest observed difference also depends on call overhead, so it is a property of "this clock plus this call on this machine", not a general resolution constant.
  - For libstdc++ 13.3 the source of `chrono.cc` shows `steady_clock::now()` built on `clock_gettime(CLOCK_MONOTONIC, ...)` when `_GLIBCXX_USE_CLOCK_MONOTONIC` is set; the VM's physical clock source is not shown by that.
  - Action: a 10 ns event cannot be timed one at a time with a step this size: time a batch of many and divide (keeping the work from being optimised away), and measure the step first on every new machine.
  - Holds for / re-check: Apple M1 Max (macOS with libc++, and a Linux VM with libstdc++). Other machines and operating systems back `steady_clock` with other timers, so every number above will differ; re-run `tick_step.cpp` there before timing anything short.
</details>

---

## Close

Next: part 3, `lesson-06-practice-bank-03-latency-spans.md` (about 21 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
