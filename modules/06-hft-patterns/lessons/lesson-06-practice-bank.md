# Lesson 06 — Practice Bank (optional): Pages, Clocks, Percentiles and Manuals

**Time: about 39 minutes** for 5 cards (4 + 12 + 10 + 8 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a guard page at `base + 4096` that `mprotect` refuses, because this Mac's page is 16384 bytes.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++); counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB1 · Recall
Q: A teammate on Linux worked out how many first-touch page faults a 1-million-element array of 48-byte records costs, and built a start-up budget on it. You port the code to a Mac. Which one number do you check first, what is it on Apple Silicon macOS (check with `getconf PAGESIZE`), and how does the count change?
   Answer format: the number, its value here, and how the fault count scales.
<details><summary>Answer</summary>
Key:

  - The page size: faults = bytes touched / page size, if each page is first touched by the array.
  - This M1 Max reports a 16384-byte page (`getconf PAGESIZE`, and `sysconf(_SC_PAGESIZE)` in the program). 48 MB / 16384 = 2929.7, and the measured count was 2930 faults.
  - A 4 KiB-page system without huge pages needs four times as many faults for the same bytes (about 11,700 here). Huge pages need far fewer.
  - Holds for / re-check: page size depends on the OS and its configuration (and on huge-page settings on Linux), so re-check per machine: `getconf PAGESIZE` or `sysconf(_SC_PAGESIZE)`, then count faults to confirm, for example with `clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/vec_growth.cpp -o "${TMPDIR:-/tmp}/vec_growth" && "${TMPDIR:-/tmp}/vec_growth" reserve` (its `minor faults in loop`). Example: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), with 4 KiB pages, the reserved array took 477 faults with transparent huge pages set to `always` and 11,719 with them disabled.
</details>

### Card M6-PB4 · Apply
Q: To catch buffer overruns at once, you put an inaccessible guard region right after a buffer: map buffer + guard as one anonymous region, then `mprotect(base + bufsize, page, PROT_NONE)`. Try bufsize = 4096 and bufsize = `sysconf(_SC_PAGESIZE)`. For the size that works, a child process writes one byte past the end. Which size works, why, and how does the child end?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/guard_page.cpp -o "${TMPDIR:-/tmp}/guard_page" && "${TMPDIR:-/tmp}/guard_page"
   ```
   Answer format: the size that works and why, then how the child ends.
<details><summary>Answer</summary>
Key:

  - The page size here is 16384. With bufsize 4096, `mprotect` at `base + 4096` fails with EINVAL: the man page says EINVAL means "addr is not a multiple of the page size".
  - With bufsize = one page, `mprotect` succeeds, and the child that writes one byte past the end is killed by signal 10 (SIGBUS here, not SIGSEGV).
  - So protection works in whole pages: a guard sits on a page boundary, and buffers that need one are padded to a page multiple. Query the page size at run time; never hard-code 4096.
  - Handle both SIGSEGV and SIGBUS if you trap such faults: which one you get is platform-specific.
  - Holds for / re-check: macOS on Apple Silicon (16 KiB pages here). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc) the page size is 4096, so both sizes worked and the child got signal 11 (SIGSEGV); other platforms were not checked. Re-check: print the page size, the `mprotect` result and the child's signal on the target.
</details>

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
  - Holds for / re-check: this Mac and macOS release; which tick the median lands on depends on machine load, and other counters have other rates. Do not trust `clock_getres` alone: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc) it reported 1 ns for all three clocks while the smallest step was 41 ns (24 MHz counter; measured with a Linux port of clock_hist.cpp that is not shipped). `clock_hist.cpp` builds on macOS only (it includes `<mach/mach_time.h>`); on another OS, replace its `mach_absolute_time` and `CLOCK_UPTIME_RAW` reads with that OS's clocks. It also reads the counter frequency with `mrs cntfrq_el0`, an AArch64-only instruction; replace that line on x86-64. Re-check: print the counter frequency and the smallest non-zero difference before trusting any per-call number.
</details>

### Card M6-PB6 · Apply
Q: A service log holds N = 50,000 request latencies. Most requests take 1 ms; the k requests that miss a cache take 9 ms. Using the nearest-rank percentile (the q-th percentile is the sample at rank ceil(q*N) in ascending order), how many slow requests can the log hold before p99.9 shows 9 ms? Before p99 does? Then check your boundaries at k = 50, 51, 500, 501:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pctl_log.cpp -o "${TMPDIR:-/tmp}/pctl_log" && "${TMPDIR:-/tmp}/pctl_log"
   ```
   Answer format: two numbers (p99.9, then p99) and the general rule.
<details><summary>Answer</summary>
Key:

  - p99.9: ceil(0.999 x 50,000) = rank 49,950, so 50 samples sit above it. Up to 50 slow requests stay invisible; the 51st moves it (the run: k = 50 gives 1 ms, k = 51 gives 9 ms).
  - p99: rank 49,500, 500 samples above it, so it needs k > 500 (k = 500: 1 ms, k = 501: 9 ms).
  - The max shows the slow path for any k >= 1.
  - General rule: with nearest-rank, the q-th percentile moves only if k > N(1 - q). Before you call a percentile "clean", compare N(1 - q) with how many slow samples you expect.
  - Holds for / re-check: pure counting, so it holds on any machine. It depends on the percentile definition: other definitions (e.g. index floor(q*N), or interpolation) move the boundary by about one sample, so state which one your tool uses.
</details>

### Card M6-PB7 · Recall
Q: An instruction costs far more inside a virtual machine than on bare metal. One likely cause is a VM exit (the CPU leaves the guest and runs the hypervisor). Using the Intel SDM (325462-093) chapter on VMX non-root operation, sections 28.1.2 and 28.1.3, sort these into "always exits" and "exits only if the hypervisor enables a control": CPUID, XSETBV, RDTSC, RDTSCP, PAUSE.
   Answer format: "always:" list, then "only with a control:" list with the control names.
<details><summary>Answer</summary>
Key:

  - Always exit: CPUID and XSETBV (listed under "Instructions That Cause VM Exits Unconditionally").
  - Exit only if a control is set: RDTSC ("RDTSC exiting"), RDTSCP ("RDTSC exiting" and "enable RDTSCP"), PAUSE ("PAUSE exiting" / "PAUSE-loop exiting").
  - Technique: when an instruction is suspiciously slow in a guest, look it up in that list first, then time it in the guest and on bare metal.
  - Holds for / re-check: Intel VMX only, per SDM 325462-093, sections 28.1.2-28.1.3; AMD SVM uses its own intercept controls, not checked. Which optional controls a given cloud enables is not in the manual. On a Linux/Intel box: time the same loop of each instruction in the guest and on bare metal, and compare.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
