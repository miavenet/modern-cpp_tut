# Lesson 06 — Practice Bank, part 5 of 12 (optional): Wakeups

**Time: about 25 minutes (estimate)** for 2 cards (10 + 15 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: sleeping to absolute deadlines, and what a blocking receive costs on Linux.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

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

---

## Close

Next: part 6, `lesson-06-practice-bank-06-linux-sockets.md` (about 25 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
