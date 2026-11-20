# Lesson 06 — Practice Bank (optional): Pages, Clocks, Percentiles and Manuals

**Time: about 278 minutes** for 31 cards (4 + 12 + 10 + 8 + 5 + 6 + 5 + 10 + 8 + 8 + 10 + 8 + 12 + 5 + 10 + 12 + 15 + 5 + 10 + 4 + 15 + 6 + 5 + 20 + 15 + 10 + 12 + 6 + 10 + 6 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a guard page at `base + 4096` that `mprotect` refuses, because this Mac's page is 16384 bytes.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

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

### Card M6-PB8 · Apply
Q: A field-bus module's datasheet says it "replies to a 16-byte command in 1.5 us". The bus runs at 1 Mb/s over 300 m of cable. Before anyone argues about it, compute: how many bits arrive in 1.5 us at that rate; how long the whole command takes to arrive; and the least time a signal needs to travel the cable out and back. Which pairs of events can the 1.5 us lie between, and which can it not? Check your numbers:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/bus_window.cpp -o "${TMPDIR:-/tmp}/bus_window" && "${TMPDIR:-/tmp}/bus_window"
   ```
   Answer format: the three numbers, then which spans are impossible and which one is possible.
<details><summary>Answer</summary>
Key:

  - 1 Mb/s x 1.5 us = 1.5 bits. The command is 128 bits and takes 128 us to arrive.
  - So 1.5 us cannot run from "first command bit leaves the master" (or arrives at the module) to "first reply bit leaves the module" for a module that reads the whole command.
  - Light in vacuum, 299 792 458 m/s, needs 1.00 us for 300 m, so at least 2.00 us out and back. Real cable is slower. So 1.5 us also cannot span "master sends" to "master sees the reply".
  - What it can span: some interval inside the module, for example from the last command bit it needs to the first reply bit leaving. That may be true and still useless for a bus budget.
  - Habit: for any headline latency, do the bits-per-window and distance-per-window arithmetic first, then ask for the start and end events.
  - Holds for / re-check: raw bit rate only; line coding and framing overhead only lower the usable bit count. Redo the arithmetic when rate, message size or cable length changes: at 10 Mb/s the same window holds 15 bits and the command takes 12.8 us. The speed of light is an exact defined constant (SI defining constants, BIPM) and does not expire. The cable is assumed to carry the signal at no more than c; real cables are slower, which only strengthens the point.
</details>

### Card M6-PB9 · Spot the trap
Q: An RPC library's page gives "one-way latency: 90 us", defined as half of a round trip timed from just before the client encodes the request to just after it decodes the reply, between two hosts. A teammate builds a budget for the span "client starts encoding the request to server finishes decoding it": 90 (one-way) + 12 (encode) + 12 (decode) + 40 (network crossing) = 154 us. Assume both directions cost the same, with 12 us to encode, 12 us to decode and 40 us for one crossing, and that there are no other costs before the server finishes decoding: all remaining round-trip time is server handling after that event and before it encodes the reply. What is wrong, and what is left of the 90 once you account for the parts already in it?
   Answer format: the error in one line, the span's figure, then what the leftover 26 us is.
<details><summary>Answer</summary>
Key:

  - By that definition the round trip contains two encodes, two decodes and two network crossings. Half of it already contains one of each.
  - So adding encode, decode and the crossing on top of the 90 counts them twice: 154 - 64 = 90, the whole published figure is surplus for this span.
  - For the span the teammate named, the given parts alone give 12 + 40 + 12 = 64 us. The 90 is not a figure for that span: under the stated assumption the round trip is 12+40+12 (request) + server handling + 12+40+12 (reply) = 180, so server handling is 52 us and half of it, 26 us, sits in the 90 but after the named span ends. Without that assumption the 26 us could equally be queueing before decode, which would sit inside the span (the span would then be 90 and the 52 us of handling would be zero): the facts alone do not place the residual, so say which assumption you make.
  - If the budget is meant for the client's full request-to-reply time, the answer is the whole round trip, 2 x 90 = 180 us, not 154. Either way, the total depends on which span the budget is for, so write the span down first.
  - You can only do this split after reading the definition. Under another definition (say, timed on the wire) the answer changes.
  - Holds for / re-check: arithmetic on invented figures (the no-other-costs-before-decode assumption in the question is what fixes the 64 us and 52 us answers); the part sizes are given, not measured. The method applies to any published "half round trip" or "one-way" figure. Re-read the definition whenever the vendor changes the test set-up.
</details>

### Card M6-PB10 · Apply
Q: `spans.cpp` lets a burst of 64 messages arrive, then one worker handles them in order. It records two spans per message: service time (handler start to handler end) and response time (message arrival to handler end). Predict: (a) for how many of the 12 800 messages is response >= service? (b) For the last message of each burst, how does its response time compare with the sum of all 64 service times? (c) Which median is larger, and what happens to the gap with a burst of 1? Run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/spans.cpp -o "${TMPDIR:-/tmp}/spans" && "${TMPDIR:-/tmp}/spans" 64 && "${TMPDIR:-/tmp}/spans" 1
   ```
   Answer format: (a), (b), (c), then the question to ask when someone quotes a latency.
<details><summary>Answer</summary>
Key:

  - (a) All 12 800. The response span contains the service span, so it can never be shorter.
  - (b) In 200 of 200 bursts the last message's response time is at least the sum of all service times in the burst. It waited for every message ahead of it.
  - (c) With a burst of 64 the median response time is far larger than the median service time (same direction in 3 runs on each of three compilers). With a burst of 1 the two medians are about equal.
  - Same code, same run, two honest numbers that differ by a large factor. A latency figure without its start and end events cannot be compared with another one.
  - When someone quotes the small one, ask: where did the clock start?
  - Holds for / re-check: (a) and (b) follow from the order of the clock reads and hold on any machine with a monotonic clock. (c) is a direction only; the size of the gap depends on burst size and handler cost, and was observed under an unknown desktop load (one machine, varies with load). Checked with Apple clang 21 (macOS arm64), and clang 18.1.3 and GCC 13.3 (Linux aarch64).
</details>

### Card M6-PB11 · Spot the trap
Q: `std::chrono::steady_clock::period` on your machine says nanoseconds. Is a 10 ns difference between two timestamps meaningful? `clock_step.cpp` takes two back-to-back `now()` readings, spins until they differ, and keeps the smallest non-zero step. It also prints `mach_timebase_info` (macOS) and the arm64 counter frequency register `cntfrq_el0`. Predict, then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_step.cpp -o "${TMPDIR:-/tmp}/clock_step" && "${TMPDIR:-/tmp}/clock_step"
   ```
   Answer format: the declared period, the smallest step, the tick from the counter, then what you do with short events.
<details><summary>Answer</summary>
Key:

  - On M1 Max (macOS) and in a Linux VM on it: declared period 1/1000000000 s, but the smallest observed step is 41 ns (3 of 3 runs).
  - `cntfrq_el0` = 24,000,000 Hz, so one tick is 41.67 ns; macOS `mach_timebase_info` = 125/3 ns per tick, the same 41.67 ns.
  - So the declared period is the unit, not the resolution. Single events shorter than about one tick cannot be timed; time many repetitions or use a cycle counter, and report the tick size with any result.
  - Holds for / re-check: Apple M1 Max hardware counter (24 MHz). Other arm64 parts and x86 TSC-based clocks differ; re-run the probe on each machine and OS.
</details>

### Card M6-PB12 · Apply
Q: `std::chrono::steady_clock::period` is `1/1000000000` on your machine, so "it measures nanoseconds". `tick_step.cpp` reads the clock twice back to back, 200,000 times, and prints how many pairs gave a zero difference, the smallest nonzero differences, what `clock_getres(CLOCK_MONOTONIC)` reports, and on arm64 the generic timer frequency register `cntfrq_el0`. Predict the smallest nonzero step, then run it. Do the four numbers agree? What should you do with a 10 ns event?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/tick_step.cpp -o "${TMPDIR:-/tmp}/tick_step" && "${TMPDIR:-/tmp}/tick_step"
   ```
   Answer format: the observed steps, the four numbers side by side, then the action.
<details><summary>Answer</summary>
Key:

  - The observed steps were 41 and 42 ns (then 83-84 ns), in 3 of 3 runs on the Mac and 3 of 3 in a Linux VM on it. Most pairs read 0 on the Mac, and a bit under half in the Linux VM; the share varies between runs.
  - `cntfrq_el0` = 24,000,000 Hz, so one tick of that counter is 41.67 ns. The observed steps are consistent with the clock advancing in 24 MHz ticks on this hardware; this does not by itself prove which timer the C++ library reads.
  - The other numbers disagree: `period` says 1 ns, `clock_getres(CLOCK_MONOTONIC)` says 1000 ns on macOS and 1 ns in the Linux VM, and the observed step is about 41.67 ns on both. So the unit (`period`), the OS-reported resolution and the observed step are three different things.
  - The smallest observed difference also depends on call overhead, so it is a property of "this clock plus this call on this machine", not a general resolution constant.
  - For libstdc++ 13.3 the source of `chrono.cc` shows `steady_clock::now()` built on `clock_gettime(CLOCK_MONOTONIC, ...)` when `_GLIBCXX_USE_CLOCK_MONOTONIC` is set; the VM's physical clock source is not shown by that.
  - Action: a 10 ns event cannot be timed one at a time with a step this size: time a batch of many and divide (keeping the work from being optimised away), and measure the step first on every new machine.
  - Holds for / re-check: Apple M1 Max (macOS with libc++, and a Linux VM with libstdc++). Other machines and operating systems back `steady_clock` with other timers, so every number above will differ; re-run `tick_step.cpp` there before timing anything short.
</details>

### Card M6-PB13 · Apply
Q: A wakeup probe sleeps for a 1 ms period 500 times and records how late each wakeup is. Variant A sleeps until absolute deadlines `start + k*period`. Variant B calls `sleep_for(period)` after each wakeup. Predict: (1) can any wakeup be early (negative lateness)? (2) After 500 periods, which variant ends far behind `start + 500*period`, and roughly by how much compared with one typical lateness? Run it three times:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/wake_probe.cpp -o "${TMPDIR:-/tmp}/wake_probe" && "${TMPDIR:-/tmp}/wake_probe"
   ```
   Answer format: (1) yes or no, (2) which variant and how far, then the rule for periodic loops.
<details><summary>Answer</summary>
Key:

  - (1) Lateness was never negative: `negative_lateness=0` in all 6 runs (3 macOS, 3 Linux).
  - (2) B drifts: its end drift is about the sum of all 500 latenesses (hundreds of times one typical lateness). A's end drift stays about one lateness, because every deadline is fixed from the start and errors do not add up. Seen in all 6 runs.
  - So a periodic probe (or a timer loop on a trading box) must use absolute deadlines, and should report the whole distribution (min, median, max), not one number.
  - Holds for / re-check: direction checked on one Mac (not isolated) and one Linux container, 3 process runs each; the microsecond values are from this machine only and are not part of the key (one machine, varies with load). The C++ form used here is `std::this_thread::sleep_until` against `steady_clock`. The standard says a timeout lasts `Dt + Di + Dm` for `_for` and returns at `Ct + Di + Dm` for `_until` (the delays are non-negative), so with an unadjusted `steady_clock` an early wakeup is not expected; the "should" wording makes it a recommendation, so the observation is the check ([thread.req.timing] p3-p4, [thread.thread.this], C++23 draft N4950).
</details>

### Card M6-PB14 · Recall
Q: A harness pins its reader and writer to chosen cores so that "same cluster" and "different cluster" can be compared. You port it to macOS on Apple silicon and replace the Linux call with the Mach affinity API. A 10-line probe calls `thread_policy_set(..., THREAD_AFFINITY_POLICY, ...)` and prints the return code. What does it print, and what does that do to the comparison?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/affinity_probe.cpp -o "${TMPDIR:-/tmp}/affinity_probe" && "${TMPDIR:-/tmp}/affinity_probe"
   ```
   Answer format: the return code, what the API promises even on success, then how to report the results.
<details><summary>Answer</summary>
Key:

  - It returns 46, `KERN_NOT_SUPPORTED` ("(os/kern) service not supported"), here on macOS 27.
  - Even where it returned success, `THREAD_AFFINITY_POLICY` takes affinity-set tags, not CPU ids, and Apple documents these as placement hints, not thread-to-processor binding (Thread Affinity API release notes). So it is not a replacement for a Linux CPU-id pin, and success would not establish pinning.
  - So on this Mac you cannot choose core placement; any "same vs different cluster" result is uncontrolled. Report it as unpinned, or run that part on Linux. If a future OS returns success, treat that as a prompt to check actual placement, not as proof of pinning.
  - The same probe on Linux (`pthread_setaffinity_np` to CPU 0) returns 0 in a container. But in a VM that selects a guest CPU: physical-cluster claims still need the host topology and placement control (card M5-PB12 in module 05).
  - Holds for / re-check: macOS 27 on M1 Max, Apple clang 21 SDK; Linux container on the same host. Apple could change behaviour later; re-run the probe on each OS release. The contract statement rests on Apple's documentation (developer.apple.com, Thread Affinity API release notes: "explicit thread to processor binding is not supported"; the notes predate Apple silicon, so they give the contract, not current support); the return code on the actual run.
</details>

### Card M6-PB15 · Apply
Q: `prefault.cpp` mallocs 64 MiB, writes one byte per page (the prefault: touch every page once during start-up so later passes find them mapped), then runs a "hot loop" three times over the same pages. It prints the page size, the number of pages, and the minor faults (`ru_minflt` from `getrusage`) for each phase. On Linux, the argument `nothp` first calls `prctl(PR_SET_THP_DISABLE, 1, ...)` (THP: transparent huge pages, 2 MiB here) and prints its return value and `PR_GET_THP_DISABLE`. Predict the fault counts on macOS, on Linux by default, and on Linux with `nothp`. Then run each 3 times, and list the per-size THP files under `/sys/kernel/mm/transparent_hugepage/`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault"
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" nothp
   ```
   Answer format: one line per platform/mode, what zero hot faults does and does not mean, then the lessons.
<details><summary>Answer</summary>
Key:

  - macOS (M1 Max): page = 16384 bytes, 4096 pages, first touch = 4096 faults (one per page), hot loop = 0. Same in 3 of 3 runs.
  - Linux VM, `nothp`: `prctl` returned 0 and `PR_GET_THP_DISABLE` read back 1 (so the request took effect); page = 4096 bytes, 16384 pages, first touch = 16383 faults (about one per page), hot loop = 0, in 3 of 3 runs.
  - Linux VM, default (top-level THP mode `[always]`): first touch = 542 faults in 3 of 3 runs, far fewer than pages, because huge pages back most of the buffer; the exact count varies. Hot loop = 0.
  - These three immediate passes recorded zero additional minor faults. That is a measurement, not a lifetime guarantee: touched pages can later be reclaimed, and after `fork` they become copy-on-write so the next write faults again. mlock(2) says writes after `fork` "will cause a page fault" and that `fork` should not follow `mlock`; real fault avoidance needs a residency/lifecycle strategy (for example `mlockall` plus no `fork`), checked by counting faults.
  - Lessons: (1) do the touching at start-up, then count faults in the hot phase instead of assuming zero; (2) never assume a page size: read `sysconf(_SC_PAGESIZE)`; (3) the fault count depends on THP, so a "faults per MiB" figure from another box does not transfer; (4) check the return value of a control call such as `prctl` before labelling a run `nothp`.
  - The top-level `enabled` file alone does not describe all THP: this VM also has per-size `hugepages-*/enabled` files (all `[never]` except `hugepages-2048kB`, `[inherit]`), and the kernel THP documentation says `MADV_COLLAPSE` ignores the sysfs settings and that the process-level `prctl` control is inherited across `fork` and `execve`.
  - Holds for / re-check: macOS on M1 Max (16 KiB pages) and one Linux VM kernel (Docker Desktop on the same Mac; 4 KiB pages, top-level THP `always`). Counts change with page size, THP mode and allocator (a large `malloc` may come straight from `mmap`). Re-check with `prefault.cpp` on the target box.
</details>

### Card M6-PB16 · Recall
Q: Which of `TCP_NODELAY`, `TCP_QUICKACK`, `TCP_CORK`, `TCP_NOPUSH` exist on Linux and on macOS, what does the Linux man page tcp(7) say about portability of `TCP_QUICKACK` and `TCP_CORK`, and how do you check the names on a new platform in two minutes?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sockopts.cpp -o "${TMPDIR:-/tmp}/sockopts" && "${TMPDIR:-/tmp}/sockopts"
   ```
   Answer format: the names per platform, the man page's words, then the two-minute check.
<details><summary>Answer</summary>
Key:

  - Header check (`sockopts.cpp`, `#ifdef` on each name, `-Wall -Wextra` clean, run on both): Linux glibc defines `TCP_NODELAY`, `TCP_QUICKACK`, `TCP_CORK` and not `TCP_NOPUSH`; macOS defines `TCP_NODELAY` and `TCP_NOPUSH` and not `TCP_QUICKACK` or `TCP_CORK`.
  - The man page marks both `TCP_QUICKACK` and `TCP_CORK` "should not be used in code intended to be portable".
  - Two-minute check: compile a file that `#ifdef`s each option name on the target, then read that platform's man page for the semantics.
  - Holds for / re-check: Ubuntu 24.04 glibc headers and the macOS SDK with Apple clang 21; tcp(7) on man7.org as opened on 2026-11-17. Nothing is claimed about network behaviour. Re-check headers and man page on each new OS or libc.
</details>

### Card M6-PB17 · Apply
Q: You want to benchmark an order check with five separately predicted branches. `checks.cpp` writes the same five range checks three ways: `ok_early` (five `if (...) return false;` lines), `ok_andand` (one `return a && b && c && d && e;`) and `ok_bitand` (the same with `&`). Predict how many conditional-branch instructions each function has at `-O2`. Then count them (the script is for arm64 listings; it counts `b.<cond>`, `cbz`, `cbnz`, `tbz`, `tbnz`, and separately compare-and-select instructions such as `ccmp` and `cset`). If you have a second compiler, repeat with it, and once at `-O0`:
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/checks.cpp -o "${TMPDIR:-/tmp}/checks.s" && sh modules/06-hft-patterns/lab/practice/count_branches.sh "${TMPDIR:-/tmp}/checks.s"
   ```
   Answer format: counts per function per compiler, then what "number of branches" should mean in a benchmark.
<details><summary>Answer</summary>
Key:

  - The source does not tell you. Five `if` lines gave 1 conditional branch with Apple clang 21 and 4 with both GCC 13.3 and clang 18.1.3, same flags, same CPU family.
  - `ok_early` and `ok_andand` compile to the same count as each other on every compiler tested at `-O2` (1 and 1, or 4 and 4). Writing `&&` instead of early returns changed nothing.
  - `ok_bitand` has 0 conditional branches on all three compilers. GCC 13.3 used `ccmp`/`cset`; both clangs used a NEON vector compare (`cmhi` + `umaxv`) plus one `cset`, and no `ccmp`. Either way there is no branch for the predictor to see.
  - At `-O0` the count follows the source (5 for `ok_early` on all three), so an unoptimized build can mislead you about the optimized one.
  - Consequence for any branch benchmark: "number of branches" has two meanings, sites (one branch instruction at one address) and instances (one execution of it). A loop that runs one `if` a million times has one site and a million instances. State which one you vary, and count the sites in the listing, because the compiler may merge or remove them.
  - Holds for / re-check: counts are for Apple clang 21.0.0 (arm64) and for GCC 13.3.0 and clang 18.1.3 (aarch64 Linux). They will differ on x86 (different instructions; the script's patterns are arm64 only) and can change with any compiler release or flag. That is the point: re-count after every toolchain or flag change.
</details>

### Card M6-PB18 · Apply
Q: A profile shows one branch mispredicted 25% of the time. A teammate says the predictor is "failing badly" on it. Before agreeing, you need a reference line. `predictor_model.cpp` simulates three simple predictors in software and counts their mistakes exactly: (a) always guess the more common outcome, (b) one 2-bit saturating counter (a tiny state machine that moves toward "taken" or "not taken" by one step per outcome), (c) a table of such counters indexed by the last 8 outcomes. It feeds them independent random outcomes taken 50%, 40%, 25%, 15% and 2% of the time, and then a repeating pattern taken-not-not-not that is also 25% taken. Predict the miss rate of (a), (b) and (c) for random 25% and for the repeating 25%. Then run:
   ```sh
   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/practice/predictor_model.cpp -o "${TMPDIR:-/tmp}/predictor_model" && "${TMPDIR:-/tmp}/predictor_model"
   ```
   Answer format: three rates for random 25%, three for the repeating pattern, then the rule.
<details><summary>Answer</summary>
Key:

  - For outcomes independent of all information the predictor has (and with no other branch exposing the next outcome), the minimum expected miss rate is min(p, 1-p), the best constant-prediction baseline; finite samples fluctuate. Always guessing the common side misses min(p, 1-p) of the time. Model output: 0.5000, 0.4000, 0.2497, 0.1499, 0.0199 for the five cases (equal to the realized taken rate, or one minus it).
  - Neither learning predictor beats that line on random outcomes. The 2-bit counter is worse: 0.2997 at p=0.25, 0.4609 at p=0.40, 0.1710 at p=0.15. The 8-outcome history table is no better (0.2997, 0.4616, 0.1708), because past random outcomes say nothing about the next one. Accept about 0.30 for both learning predictors on random 25% input.
  - So 25% misses on a branch that is taken 25% of the time, with random outcomes the predictor cannot otherwise see, is already about the best available. The predictor is not failing; the data is unpredictable. For a real profile treat min(p, 1-p) as the baseline to compare against, not as an unconditional bound: another branch may reveal the next outcome.
  - The repeating pattern has the same 25% taken rate but a different answer: majority guess 0.2500, 2-bit counter 0.2500, history table 0.0000 (3 warm-up misses). Structure in the order of outcomes, not the taken rate, is what a history predictor uses.
  - Rule: judge a measured miss rate against the min(p, 1-p) baseline for that branch, not against zero. If the rate is far below that line, the outcomes have a pattern the predictor learned. Then ask whether your benchmark input has that same pattern in production.
  - Holds for / re-check: a software model with a fixed seed, so the mistake counts are deterministic and were identical with Apple clang 21 and GCC 13.3 (Linux); displayed rates are rounded to four decimals. It says nothing about how large or clever a real CPU's predictor is. The min(p, 1-p) line is arithmetic and does not expire. Whether a real core reaches it, or learns a given pattern, depends on the CPU and must be measured there with a mispredict counter.
</details>

### Card M6-PB19 · Apply
Q: `sorted_shuffled.cpp` sums the bytes >= 128 in a 1 MiB array, 200 times, once with the array sorted and once shuffled (same values). Build A: plain `-O2`. Build B: `-O2 -DKEEP_BRANCH`, which puts an empty `asm volatile("")` inside the `if`. First read the `sum_big` listing of each build and say whether a per-element branch survives. Then predict for each build whether shuffled is slower than sorted, and run each three times:
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o -
   clang++ -std=c++20 -O2 -DKEEP_BRANCH -S modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_a" && clang++ -std=c++20 -O2 -Wall -Wextra -DKEEP_BRANCH modules/06-hft-patterns/lab/practice/sorted_shuffled.cpp -o "${TMPDIR:-/tmp}/ss_b"
   for r in 1 2 3; do for b in ss_a ss_b; do "${TMPDIR:-/tmp}/$b" sorted; "${TMPDIR:-/tmp}/$b" shuffled; done; done
   ```
   Answer format: what each listing shows, the direction per build, then the trap.
<details><summary>Answer</summary>
Key:

  - Listing: Build A's `sum_big` is vectorized (16 `uaddw`-family SIMD adds, no `tbz`); Build B's has no `uaddw` and a per-byte `ldrsb` + `tbz` test-and-branch.
  - Build B: shuffled was slower than sorted in 3 of 3 runs. Build A: no consistent order penalty at the printed precision. Grade branch presence and direction only; no magnitude is part of the key (one machine, varies with load).
  - So "sorted vs random input" only exposes branch prediction if the branch survives compilation. Check the listing first.
  - Trap: without the `asm volatile` clobber in the repeat loop, build A printed `0.000` ns per element and the listing had a single `bl sum_big` in `main`, outside the 200-repetition loop: the compiler hoisted the call and reused the result (so the printed value is rounded amortised output, not literally no work). The checksum was still right; only the time showed the problem.
  - Holds for / re-check: direction only, M1 Max, Apple clang 21 `-O2`, 3 process runs. Other compilers or flags may if-convert differently (or not vectorize); re-check the listing and the direction on the target.
</details>

### Card M6-PB20 · Apply
Q: `__builtin_prefetch(p, rw, locality)` takes rw = 0 (read) or 1 (write) and locality 0-3. Predict what `prefetch_five.cpp`'s five forms (read with locality 3, 2, 1, 0; write with 3) compile to on arm64 and on x86-64, then check. What happens to the write form on generic x86-64, on GCC as well as Clang, and what if you pass the locality from a variable (`prefetch_var.cpp`)?
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/prefetch_five.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/06-hft-patterns/lab/practice/prefetch_five.cpp -o -
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/prefetch_var.cpp -o -
   ```
   (Repeat the x86-64 line with `-mprfchw`, `-march=broadwell` and `-march=x86-64`.)
   Answer format: the arm64 list, the x86-64 list, what the write form becomes, then the variable-locality result.
<details><summary>Answer</summary>
Key:

  - arm64: `prfm pldl1keep`, `pldl2keep`, `pldl3keep`, `pldl1strm`; write: `prfm pstl1keep`.
  - x86-64: `prefetcht0`, `prefetcht1`, `prefetcht2`, `prefetchnta`. Write form with generic x86-64 (and `-march=x86-64-v3`): plain `prefetcht0`; it becomes `prefetchw` only when the target has PRFCHW, for example `-mprfchw` or a `-march` that includes it: `-march=broadwell`, `-march=skylake` and `-march=znver2` did on GCC 13.3 and Clang 20.1 (Compiler Explorer); `-march=x86-64` did not. Other targets: check.
  - GCC 13.3 x86-64 (Compiler Explorer, `-O2 -std=c++20`): the same five instructions, with the write form `prefetcht0`; with `-mprfchw` it is `prefetchw`.
  - A non-constant locality is a compile error: Clang says "argument to '__builtin_prefetch' must be a constant integer"; GCC 13.3 says "third argument to '__builtin_prefetch' must be a constant".
  - So the write hint silently degrades to a read hint unless the target flags allow it: read the asm for your build flags.
  - Holds for / re-check: Apple clang 21.0.0, targets arm64-apple-macos and x86_64-apple-macos (compile only, nothing x86 was run); GCC 13.3 x86-64 on Compiler Explorer. New `-march` levels may add PREFETCHW; re-check with `-S`.
</details>

### Card M6-PB21 · Apply
Q: Predict the instruction each `__builtin_prefetch` form becomes on arm64 and on baseline x86-64: `(p,0,0)`, `(p,0,1)`, `(p,0,2)`, `(p,0,3)` and `(p,1,3)`. Then build `prefetch_forms.cpp` for both targets, and once more for x86-64 with `-mprfchw` added. Which hint is silently weakened on baseline x86-64, and how many warnings do you get?
   ```sh
   clang++ --target=arm64-apple-macos -std=c++20 -O2 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu modules/06-hft-patterns/lab/practice/prefetch_forms.cpp -o -
   ```
   Answer format: the five arm64 instructions in order, the five x86-64 ones, then the weakened hint and the habit.
<details><summary>Answer</summary>
Key:

  - arm64: `(p,0,0)` -> `prfm pldl1strm`; `(p,0,1)` -> `prfm pldl3keep`; `(p,0,2)` -> `prfm pldl2keep`; `(p,0,3)` -> `prfm pldl1keep`; `(p,1,3)` -> `prfm pstl1keep` (the "st" form is the write hint).
  - x86-64 baseline: `prefetchnta`, `prefetcht2`, `prefetcht1`, `prefetcht0`, and the write hint `(p,1,3)` also becomes plain `prefetcht0`. With `-mprfchw` it becomes `prefetchw`.
  - GCC 13.3 x86-64 (Compiler Explorer, `-O2`): same mapping, write hint `prefetcht0`, and `prefetchw` with `-mprfchw`.
  - So the write intent is dropped without a warning (0 warnings with `-Wall -Wextra`) unless the target has the PRFCHW feature enabled: the compiler still emits a read prefetch, it does not drop the instruction. (GCC's documented "no other code is generated" rule is for targets with no prefetch at all; it is not what happened here.)
  - Habit: read the assembly for the exact `-march` you ship before you trust a hint.
  - Holds for / re-check: Apple clang 21.0.0 (arm64-apple-macos, and x86-64 compile only) and GCC 13.3 x86-64 on Compiler Explorer; GCC's `__builtin_prefetch` documentation (gcc.gnu.org, Other-Builtins) gives locality 0-3, default 3. Other compilers and versions may map locality differently; re-check with `-S`. Whether the hint helps speed was not measured.
</details>

### Card M6-PB22 · Recall
Q: A code review says "this prefetch guarantees the line is in L1 when we read it, so the read cannot miss." Using the Intel SDM entry for PREFETCHh (Vol. 2B), list what the architecture actually promises, and say how you would check the effect on a new CPU.
   Answer format: three short points from the manual, then the check.
<details><summary>Answer</summary>
Key:

  - It is "merely a hint and does not affect program behavior".
  - Locality hints are "implementation-dependent, and can be overloaded or ignored by a processor implementation"; the amount fetched is implementation-dependent (at least 32 bytes).
  - It is not ordered with respect to fences or locked instructions; prefetches to uncacheable or WC memory are ignored.
  - So nothing is guaranteed: the effect is a property of the CPU model, and can change with the next one. Check by timing with the prefetch on and off, with a checksum gate (as in card M6-PB23), on each target CPU.
  - Holds for / re-check: Intel 64 architecture text only (SDM). Other ISAs have their own wording; arm64 `prfm` was not looked up here.
</details>

### Card M6-PB23 · Apply
Q: `pf_gather.cpp` does 8M random reads `sum += table[idx[i]]` from a table of 2^18 bytes (fits in cache) or 2^28 bytes (does not), optionally prefetching `table[idx[i+16]]`. `pf_chase.cpp` does dependent reads, where each index comes from the previous value. Predict (a) the direction of the prefetch effect at each size (faster, slower, no clear change), (b) how chase compares with gather at 2^28, and say what in the loop body could make your prediction wrong on other hardware. Run both in rotated order 3 times, and check that checksums match between prefetch on and off:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pf_gather.cpp -o "${TMPDIR:-/tmp}/pf_gather" && for r in 1 2 3; do "${TMPDIR:-/tmp}/pf_gather" 18 0; "${TMPDIR:-/tmp}/pf_gather" 18 16; "${TMPDIR:-/tmp}/pf_gather" 28 16; "${TMPDIR:-/tmp}/pf_gather" 28 0; done
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/pf_chase.cpp -o "${TMPDIR:-/tmp}/pf_chase" && for r in 1 2 3; do "${TMPDIR:-/tmp}/pf_chase" 28; done
   ```
   Answer format: (a) one direction per size, (b) one line, then the lesson.
<details><summary>Answer</summary>
Key:

  - Directions only (one machine, varies with load; no figures are claimed): 2^18 table: prefetch on was slower in every pair (extra instructions for data already in cache). 2^28 table: prefetch on was never clearly faster, so no win here.
  - Chase at 2^28 was far slower per read than gather at 2^28 in every run. Likely explanation (reasoned, not measured here): independent misses overlap in the out-of-order core (memory-level parallelism), dependent ones cannot.
  - The no-win result is specific to this loop body (two instructions per lookup) and this core. Where there is more work per lookup, or a core that cannot keep enough misses in flight, a prefetch can win: test, do not assume.
  - Lesson: a prefetch helps only when the CPU cannot already see the future address early enough. Predict, measure on your machine, and keep a checksum gate so both versions do the same work.
  - Holds for / re-check: one Apple M1 Max, macOS, Apple clang 21.0.0 `-O2`, unpinned; directions only. A CPU with a smaller out-of-order window, more work per element, or a different distance can change the result. Re-check by sweeping the distance (0, 4, 16, 64) and the table size on the target box.
</details>

### Card M6-PB24 · Apply
Q: `musttail_feat.cpp` prints nothing; it stores `__has_cpp_attribute(clang::musttail)` and `__has_cpp_attribute(gnu::musttail)` in two globals and has one function whose `return` carries `[[clang::musttail]]` (a Clang attribute that makes the tail call a requirement: compile error if it cannot be done). Predict both values and whether the file compiles with GCC 13, GCC 14, GCC 15 and clang 18/20. Check on Compiler Explorer (x86-64, `-O1 -std=c++20`) and read the globals in the listing; locally:
   ```sh
   clang++ -std=c++20 -O1 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/musttail_feat.cpp -o -
   ```
   What text does GCC print, does it name the attribute, and what one flag makes it a build error?
   Answer format: one line per compiler, then the flag and the habit.
<details><summary>Answer</summary>
Key:

  - GCC 13.3 and 14.2 (x86-64): compiles; both globals are 0 (`.zero 4`); warning `attributes at the beginning of statement are ignored [-Wattributes]`. The text does not say `musttail`. With `-Werror=attributes` it is an error (`cc1plus: some warnings being treated as errors`).
  - GCC 15.3: both values are 1 (`.long 1`), no warning: the attribute is accepted under both spellings.
  - clang 18.1 and 20.1: `clang::musttail` is 1, `gnu::musttail` is 0, no warning.
  - Lesson: an ignored statement attribute is a warning on a line that does not name it. Feature-test with `__has_cpp_attribute(...)` (the value, not just the spelling, differs per compiler) and put `-Werror=attributes` on, or `#error` when the macro is 0, so a build without the guarantee fails instead of silently producing ordinary calls.
  - Holds for / re-check: Compiler Explorer, x86-64: GCC 13.3, 14.2, 15.3, clang 18.1.0 and 20.1.0, compile only, nothing run. Which compilers define which spelling changes between releases; re-run the same two-line test on any new compiler.
</details>

### Card M6-PB25 · Spot the trap
Q: Which of these does Apple clang 21 accept under `[[clang::musttail]]`? (a) `int ok(int a, int b) { [[clang::musttail]] return two(b, a); }` with `int two(int, int)`; (b) `int fewer(int a, int b) { [[clang::musttail]] return one(a + b); }` with `int one(int)`; (c) `int widened(int a, int b) { [[clang::musttail]] return (int)wide(a + b); }` with `long wide(int)`; (d) a function that copies an argument into a local, reads it through a pointer, then `[[clang::musttail]] return two(*p, b);`.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/06-hft-patterns/lab/practice/musttail_rules.cpp -o "${TMPDIR:-/tmp}/musttail_rules.o"
   ```
   Answer format: which compile, the two error messages, then the design rule.
<details><summary>Answer</summary>
Key:

  - (a) and (d) compile.
  - (b) error: "cannot perform a tail call to function 'one' because its signature is incompatible with the calling function", note "different number of parameters (expected 2 but has 1)".
  - (c) error: "'clang::musttail' attribute requires that the return value is the result of a function call" (a cast is extra work after the call).
  - Design rule: give every handler in a tail-call chain the same signature, and return the call's result unchanged.
  - Holds for / re-check: Apple clang 21.0.0, `-std=c++20 -O2 -Wall -Wextra`. Clang's rules for musttail have changed across versions; re-check by compiling the same four functions.
</details>

### Card M6-PB26 · Apply
Q: (Linux only: run it on Linux or in a Linux container.) Write a program that sends 64 small UDP datagrams over loopback, either one `sendto()` per datagram or one `sendmmsg()` with 64 entries, then drains them with `recvmmsg(..., MSG_DONTWAIT)`, and prints how many send and receive calls it made. Your receive loop must (1) treat `EINTR` and `EAGAIN`/`EWOULDBLOCK` as retryable, any other error as fatal, and (2) give up after an overall deadline, because UDP can lose datagrams and the loop would otherwise hang. Your send loop must handle `sendmmsg` accepting fewer entries than asked. Compare with the reference `udp_batch.cpp`, then count kernel entries with strace:
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/udp_batch.cpp -o "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" batch
   ```
   Answer format: send and receive calls for each mode, the strace counts, then how to grade the send count and the error handling.
<details><summary>Answer</summary>
Key:

  - One-by-one: 64 `sendto` calls. Batched: 1 `sendmmsg` call, accepted in full. Both: 64 datagrams arrive, and 1 `recvmmsg` call drained all 64, with 0 empty receive calls (3 runs each, identical).
  - strace agrees: 64 sendto + 1 recvmmsg, versus 1 sendmmsg + 1 recvmmsg.
  - Technique: count kernel entries with `strace -c` before and after a change. When the batch is accepted in full, the number of send calls falls from the datagram count to 1. If `sendmmsg` accepts only part of the batch, you need more calls (sendmmsg(2): it returns the number of messages sent, and if that is less than requested the caller can retry with the rest), so grade the send-call count against the number of batches actually accepted, not a fixed factor.
  - Error handling: `recvmmsg` returns -1 with `errno` set on error (recvmmsg(2)); a non-blocking receive with nothing queued gives `EAGAIN`/`EWOULDBLOCK` (recv(2)), which is retryable; an overall deadline turns a lost datagram into a reported failure instead of a hang.
  - Holds for / re-check: Linux only (`sendmmsg`/`recvmmsg` are Linux calls; macOS has neither): Ubuntu 24.04 aarch64 container, GCC 13.3, loopback only. A real NIC and full socket buffers can return partial batches, so only the full-acceptance case was measured. Re-check with strace on the target host.
</details>

### Card M6-PB27 · Apply
Q: (Linux only.) `recv_wakeups.cpp`: a sender thread sends 200 one-byte datagrams about 0.2 ms apart. The receiver thread either calls blocking `recv()` or spins on `recv(..., MSG_DONTWAIT)`. Each mode prints the receiver thread's voluntary context switches (the thread gave up the CPU itself, usually to sleep) and involuntary ones (`getrusage(RUSAGE_THREAD)`), and how many `recv` calls came back empty. Predict the relationship between the modes, then run each mode several times untraced. In a separate run under strace, count the receive syscalls:
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra -pthread modules/06-hft-patterns/lab/practice/recv_wakeups.cpp -o "${TMPDIR:-/tmp}/recv_wakeups" && for i in 1 2 3 4 5; do "${TMPDIR:-/tmp}/recv_wakeups"; "${TMPDIR:-/tmp}/recv_wakeups" spin; done
   strace -f -c -e trace=recvfrom "${TMPDIR:-/tmp}/recv_wakeups" spin
   ```
   Answer format: the relationship between modes (not exact figures), the traced syscall count, then why the two runs are kept separate.
<details><summary>Answer</summary>
Key:

  - Relationship, not exact figures: blocking has many voluntary switches (about one per message here: 199 in 5 of 5 runs) and no empty calls; a blocking `recv` can also return datagrams already queued without sleeping, so do not expect exactly one sleep per message. Spinning has zero voluntary switches and a very large number of empty polls (hundreds of thousands here: 237090 to 239794 over 5 runs on this one machine; the number depends on CPU speed and gap).
  - Spinning still consumes CPU while runnable, so it can still be switched out: involuntary switches were 0 in four of five runs and 1 in one run. Do not grade on an exact 0.
  - Mechanism: spinning removes the sleeps but not the kernel. Under strace the spinning receiver made one `recvfrom` syscall per attempt (in one run 1869 calls = 200 received + 1669 errors), versus 200 for blocking. Polling a socket is not kernel bypass; removing the calls takes a different interface, verified separately with `strace -c`.
  - Tracing changes the thing measured: that traced spin run showed 3740 voluntary switches and only 1669 empty polls, because tracing itself perturbs scheduling. Collect context-switch deltas untraced and count syscalls in a separate traced run.
  - If another thread shares the spinning thread's core, a rise in the involuntary count is possible; check on a busy machine.
  - Holds for / re-check: Linux only (`RUSAGE_THREAD` is Linux-specific): Ubuntu 24.04 aarch64 container, loopback, idle 10 CPUs, one machine. The example counts are not graded; the empty-poll count scales with message gap and CPU speed, so compare modes, not numbers across machines. Re-check on the target host with the same program.
</details>

### Card M6-PB28 · Apply
Q: (Linux only.) A wiki says "reading the clock is free on Linux: `clock_gettime` never enters the kernel." `clock_ids.cpp` makes 1000 `clock_gettime` calls with one clock id chosen by index on the command line: 0 REALTIME, 1 MONOTONIC, 2 MONOTONIC_RAW, 3 BOOTTIME, 4 PROCESS_CPUTIME_ID, 5 THREAD_CPUTIME_ID. `clock_vs_getppid.cpp` also makes 1000 `getppid()` calls. Count the system calls for each (the vDSO is a small library the kernel maps into every process so some calls can run without entering the kernel). Which claims hold, and what else could break them?
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_ids.cpp -o "${TMPDIR:-/tmp}/clock_ids" && for id in 0 1 2 3 4 5; do strace -c -e trace=clock_gettime "${TMPDIR:-/tmp}/clock_ids" $id; done
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_vs_getppid.cpp -o "${TMPDIR:-/tmp}/clock_vs_getppid" && strace -c "${TMPDIR:-/tmp}/clock_vs_getppid"
   ```
   Answer format: syscalls per clock id and for `getppid`, then what the claim depends on and how to re-check.
<details><summary>Answer</summary>
Key:

  - REALTIME, MONOTONIC, MONOTONIC_RAW, BOOTTIME: 0 syscalls (served by the vDSO). PROCESS_CPUTIME_ID and THREAD_CPUTIME_ID: 1000 syscalls each. `getppid()`: 1000 syscalls.
  - So the claim depends on the clock id. It also depends on the kernel and the clock source: the vdso(7) man page records kernels and architectures where some clocks fall back to the real system call. This container's clock source was `arch_sys_counter`.
  - Re-check on the target host: run the same strace count, and read `/sys/devices/system/clocksource/clocksource0/current_clocksource`.
  - Holds for / re-check: Linux only: kernel 7.0.12-linuxkit, aarch64, glibc (Ubuntu 24.04 container). Results can differ on other kernels, architectures, clock sources and under some virtualization; re-measure there.
</details>

### Card M6-PB29 · Apply
Q: `prefetch_six.cpp` has six one-line functions, each calling `__builtin_prefetch(p, rw, locality)` with (rw, locality) = (0,3), (0,2), (0,1), (0,0), (1,3), (1,0). Before you look, predict the instruction for each on arm64 and on x86-64. Then compile for arm64, and on Compiler Explorer for x86-64 with these flag sets: `-O2`, `-O2 -march=x86-64-v3`, `-O2 -mprfchw`, `-O2 -march=skylake`, `-O2 -march=znver3`. Which flag sets keep the write intent (a request made in anticipation of a write), and how would you notice if yours did not?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/06-hft-patterns/lab/practice/prefetch_six.cpp -o - | grep -E 'prefetch|prfm'
   ```
   Answer format: the arm64 list, the x86-64 read list, which flag sets give `prefetchw`, then why and how you notice.
<details><summary>Answer</summary>
Key:

  - arm64 (Apple clang 21 and GCC 13.3 agree): reads become `prfm pldl1keep`, `pldl2keep`, `pldl3keep`, `pldl1strm`; writes become `prfm pstl1keep` and `pstl1strm`. Write intent is kept.
  - x86-64 reads (gcc 13.3 and clang 18.1, all flag sets): `prefetcht0`, `prefetcht1`, `prefetcht2`, `prefetchnta`.
  - x86-64 writes: `prefetchw` only with `-mprfchw`, `-march=skylake` or `-march=znver3`. With plain `-O2` and with `-march=x86-64-v3` the write prefetch silently becomes a read prefetch (`prefetcht0` / `prefetchnta`). No warning.
  - Why: `PREFETCHW` has its own CPUID feature flag, so the compiler only emits it when the target says the CPU has it. GCC documents no generated prefetch and no warning when the target lacks data-prefetch support entirely. The read-prefetch fallback for these write requests is an observation of these compiler/flag combinations.
  - The trap: `x86-64-v3` includes AVX2, so "we build for modern CPUs" does not imply write prefetch. You notice only by grepping the assembly for `prefetchw`.
  - Write intent does not guarantee that the line is fetched with, or keeps, exclusive ownership.
  - Holds for / re-check: gcc 13.3 and clang 18.1 (x86-64, Compiler Explorer, compile only), Apple clang 21 and GCC 13.3 (arm64), checked 2026-11-18. The flag-to-instruction mapping can change with compiler version or `-march` level definitions. The Intel manual says locality hints are implementation-dependent and may be ignored, so the instruction you get says nothing about the speed you get: that needs a measurement on the target CPU. Re-check: rerun the six functions with your real build flags and `grep -E 'prefetch|prfm'`.
</details>

### Card M6-PB30 · Spot the trap
Q: A library chooses its fast path like this:
   ```cpp
   #ifdef __AVX2__
     use_fast_path();
   #else
     use_portable_path();
   #endif
   ```
   The team says "this picks the right path for whatever CPU the binary lands on." Is that true? Build `modules/06-hft-patterns/lab/practice/avx2_macro.cpp` (prints which branch the preprocessor chose, then calls `__builtin_cpu_supports("avx2")`) on Compiler Explorer with x86-64 gcc, flags `-O2` and then `-O2 -mavx2`, and compare the assembly.
   Answer format: true or false, what each binary contains, what the runtime call does, then the fix.
<details><summary>Answer</summary>
Key:

  - False. `__AVX2__` is set by the build flags, not by the CPU that runs the binary. With `-O2` the binary contains only the string "built without __AVX2__"; with `-O2 -mavx2` it contains only "built with __AVX2__ defined". The other branch is not in the binary at all.
  - `__builtin_cpu_supports("avx2")` is different: the assembly calls `__cpu_indicator_init` and reads `__cpu_model` at run time, so it answers for the CPU it runs on.
  - Consequences: a plain build never takes the fast path, even on a CPU that has AVX2; in a `-mavx2` build the AVX2 branch is selected at compilation, and execution on an incompatible CPU is not guaranteed.
  - Fix: decide at run time once (for example with `__builtin_cpu_supports` at start-up, stored in a function pointer), or build both paths and dispatch. Compile the dispatcher and fallback for the deployment baseline; compile the AVX2 implementation separately with `-mavx2` or a supported per-function target attribute, then dispatch after the runtime feature check. Adding a check to a translation unit globally built with `-mavx2` does not make it portable to non-AVX2 CPUs, because the compiler may emit AVX2 instructions anywhere in it. A compile-time macro only says what you allowed the compiler to use.
  - Holds for / re-check: x86-64 gcc 13.3 on Compiler Explorer (its runs show behaviour on that host only, not on an incompatible CPU). `__builtin_cpu_supports` is a GCC/clang extension for x86 (Arm targets have different names and feature sets). Re-check: grep your release binary's assembly for the string or symbol of each path.
</details>

### Card M6-PB31 · Recall
Q: Write the layers of a read-only compatibility report for "can this Linux box reserve part of the L3 for one thread?" (cache partitioning: Intel calls it Cache Allocation Technology, AMD L3 Cache Allocation Enforcement; *resctrl* is the Linux filesystem that exposes it). Keep four things apart: CPU enumeration, kernel support, mount exposure and permissions. Give the CPUID order for Intel, including the asymmetric case. Then run `rdt_probe.sh` (it counts RDT flags in `/proc/cpuinfo`, checks `/proc/filesystems` for resctrl, and lists `/sys/fs/resctrl`) in an arm64 Linux container and say what it can and cannot show.
   ```sh
   bash modules/06-hft-patterns/lab/practice/rdt_probe.sh
   ```
   Answer format: Intel CPUID order, AMD check, kernel checks, the container result, then what the probe cannot show.
<details><summary>Answer</summary>
Key:

  - CPU, Intel (check leaf and sub-leaf availability first; query per logical processor): asymmetric first. If CPUID.(7,1):ECX[1] is set, leaf 28H is the selected leaf and may differ between logical processors. Otherwise, if CPUID.(7,0):EBX[15] (`RDT_A`) is set, leaf 10H is selected. In the selected leaf's resource bitmap, test the L3 bit (leaf 10H sub-leaf 0 EBX[1] `CAT_L3`; leaf 28H has the same definitions). Asymmetric leaves can exist without informative symmetric ones, and symmetric platforms can enumerate asymmetric leaves, so neither set alone decides.
  - CPU, AMD: CPUID Fn0000_0007 EBX bit 15 (`PQE`), then leaf 10H for the L3 resource; the class count is MAX_COS+1 and the mask length CBM_LEN+1.
  - Kernel: flags such as `rdt_a`, `cat_l3`, `cdp_l3`, `mba` in `/proc/cpuinfo`; `resctrl` listed in `/proc/filesystems`; mount it (`mount -t resctrl resctrl /sys/fs/resctrl`) and read `info/L3/cbm_mask`, `num_closids` and `sparse_masks` for the real limits.
  - Container result: 0 RDT flags, no resctrl filesystem, no `/sys/fs/resctrl`. This container exposes no listed RDT flags or resctrl interface; the probe alone cannot distinguish absent hardware, unavailable kernel support/modules, or hidden/unmounted/inaccessible interfaces. It is an exposure probe, not proof of CPU incapability, and a mounted resctrl is not proof of an exclusive reservation. It shows nothing about any x86 server.
  - Report each layer separately, and run it in the environment you deploy on: a VM or container may hide what the host CPU has.
  - Holds for / re-check: the x86 parts (Intel CPUID, AMD, kernel resctrl) are citations from the Intel SDM, the AMD APM and kernel.org, not results: nothing was run on x86. The only run is the arm64 Linux container (Ubuntu 24.04, linuxkit kernel 7.0.12), as an exposure result. The asymmetric enumeration shows the CPUID recipe itself changes over time. Arm has its own partitioning architecture (not covered by these sources, not checked). Re-check: rerun `rdt_probe.sh` on the target and reread the current SDM CPUID tables.
</details>

### Card M6-PB32 · Apply
Q: A decoder precomputes the result for its most common opcode. `heldout_routes.cpp` chooses between opcodes 2 and 9 using training input {2,2,2,2,2,2,2,9}, then counts special-path hits on a separate input {2,9,9,9,9,9,2,9}. Other opcodes use the correct fallback. Predict the chosen opcode, both hit counts and the result mismatches. Run it before deciding whether the training hit count justifies deploying that specialization:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/heldout_routes.cpp -o "${TMPDIR:-/tmp}/heldout_routes" && "${TMPDIR:-/tmp}/heldout_routes"
   ```
   Answer format: chosen opcode, the hit counts and mismatches, then one line on what the held-out input shows.
<details><summary>Answer</summary>
Key:

  - It chooses 2: training hits=7 of 8, held-out hits=2 of 8. Specializing 9 instead would give 6 held-out hits.
  - The 7-to-2 drop comes from the different input distributions. The held-out input stands in for the deployed feed; holding it out is what lets the shift show, it does not cause it.
  - The result mismatch count is 0: passing correctness does not imply the specialized path stays common.
  - Keep the evaluation input out of the selection step. If you use its six-hit result to retune the candidate to 9, it has become training data and you need another held-out evaluation.
  - These are source-level route counts, not instruction counts or a speed ranking; the compiler can simplify this toy calculation.
  - Holds for / re-check: a deterministic toy decoder (Apple clang 21, C++20). Held-out means input not used to select the candidate. Counts change with the input distribution; after a feed change, check correctness and path frequency separately.
</details>

### Card M6-PB33 · Spot the trap
Q: A toy trace collector uses the sum of two function IDs as its path key. Its event sequence is {(2,5),(2,5),(5,2),(2,5),(5,2)}. A second collector keys on the full ordered pair. Predict the record counts and event counts of each. Run `path_identity.cpp`, then explain why a correct grand total cannot validate path attribution:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/path_identity.cpp -o "${TMPDIR:-/tmp}/path_identity" && "${TMPDIR:-/tmp}/path_identity"
   ```
   Answer format: records and events for each collector, then the trap, then the fix.
<details><summary>Answer</summary>
Key:

  - Sum-key collector: 1 record with 5 events. Full-pair collector: 2 records, (2,5) with 3 and (5,2) with 2.
  - Both account for all five events, yet one merges distinct routes. A total-count check misses this attribution failure.
  - For this toy collector, use the full pair as identity. In a hashed design, compare full keys when hashes match; a wider hash alone is not an exact-identity check.
  - A forced collision belongs in the collector's tests, along with a check of total events.
  - Holds for / re-check: Apple clang 21, C++20; a deterministic two-function trace model, no production profiler, no timing. Re-check on longer paths and recursion. The toy sum is deliberately poor and says nothing about any real profiler's collision rate.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
