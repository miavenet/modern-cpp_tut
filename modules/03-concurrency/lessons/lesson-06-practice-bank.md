# Lesson 06 — Practice Bank (optional): Atomics on Real Hardware, Waiting and Waking

**Time: about 48 minutes** for 5 cards (12 + 4 + 20 + 6 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: an atomic wait that never sleeps at a 2 us gap because it burns a whole core, and an acquire load that compiles to the same instruction as seq_cst on three of the four ARM targets tried here.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB1 · Apply
Q: Four tiny functions test whether an atomic 64-bit word equals `k`: acquire load + `==`, seq_cst load + `==`, relaxed compare-exchange that writes back `k`, seq_cst compare-exchange. Using only compiler builtins (no library headers; only the compiler's freestanding `<stdint.h>`), emit assembly for three targets from the one clang on your Mac. Then compile the same four functions written with `std::atomic` natively three times: no flag, `-march=armv8-a`, `-mcpu=apple-a7`. Predict first: on which targets does the memory order change the instruction?
   ```sh
   clang -O2 -S -ffreestanding --target=x86_64-linux-gnu modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8.1-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_probes.cpp -o -
   ```
   (Repeat the last line with `-march=armv8-a`, then with `-mcpu=apple-a7`.)
   Answer format: one line per target, then your answer to the prediction.
<details><summary>Answer</summary>
Key:

  - x86-64: both loads are a plain `movq`; both CAS orders are the same `lock cmpxchgq`.
  - AArch64 (ARMv8.0): loads become `ldar` (acquire and seq_cst). The CAS is a load-exclusive / store-exclusive retry loop (`ldxr`/`stxr` relaxed, `ldaxr`/`stlxr` seq_cst). The order changes the CAS instructions, not the load.
  - AArch64 with ARMv8.1 atomics: the CAS is a single `cas` (relaxed) or `casal` (seq_cst); both loads are still `ldar`.
  - Native on this Mac (Apple clang default target): acquire load `ldapr`, seq_cst load `ldar`, relaxed CAS `cas`, seq_cst CAS `casal`. Adding `-march=armv8-a` changes nothing: the driver still passes `-target-cpu apple-m1` (check with `-###`), so the v8.1 atomics stay on. The lever is `-mcpu`: `-mcpu=apple-a7` gives `ldar` for both loads and retry loops (`ldxr`/`stxr` relaxed, `ldaxr`/`stlxr` seq_cst).
  - Answer to the prediction: CAS: the order changes the instruction on every AArch64 target (retry loop `ldxr/stxr` vs `ldaxr/stlxr`, or `cas` vs `casal`). Loads: acquire and seq_cst are the same `ldar` on ARMv8.0, ARMv8.1 and apple-a7; only the native apple-m1 target gives acquire its own `ldapr`. x86-64: no change for any of the four.
  - Lesson: a claim about what an atomic costs is a claim about one ISA and one target CPU. Read the instructions per target, and check which CPU the driver really targets.
  - Holds for / re-check: Apple clang 21 codegen and driver defaults. Other compilers or versions can pick other sequences and other default CPUs. Example: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), both compilers default to outline atomics (clang: `-target-cpu generic`, `+outline-atomics`), so the CAS is a call to a helper (`bl __aarch64_cas8_relax`, `bl __aarch64_cas8_acq_rel`), not an inline instruction; GCC with `-march=armv8.1-a` gives `cas`/`casal`. Re-check by re-emitting the same files with your compiler, `-march`/`-mcpu`, and `-###` to see the CPU the driver passes.
</details>

### Card M3-PB2 · Recall
Q: The C++ standard and the Intel manual describe a compare-exchange whose comparison fails in different terms. What does each say happens to memory, and why does the difference matter for "it failed, so nothing was written"? First attempt: look it up in the compare_exchange Effects of [atomics.types.operations] and the CMPXCHG page of the Intel SDM (325462-093); on reviews, recall it.
   Answer format: one line for C++, one for x86, one for why it matters.
<details><summary>Answer</summary>
Key:

  - C++: the standard keys this on the return value. If the operation returns true, it is an atomic read-modify-write; otherwise (including a spurious failure of the weak form, which can return false even when the values are equal) it is an atomic load, and `expected` gets the value read.
  - Intel SDM, CMPXCHG: the destination "receives a write cycle without regard to the result of the comparison"; on failure the old value is written back. "The processor never produces a locked read without also producing a locked write."
  - So the language-level meaning (a load) and the x86 hardware cost (a locked write to the line) differ. Use the language rules for correctness and the manual for cost.
  - Holds for / re-check: the C++ wording is in the N5054 draft and earlier drafts; the write-cycle sentence is Intel x86 (SDM 325462-093, CMPXCHG), and other ISAs use other instructions (see card M3-PB1). Re-check in the current SDM CMPXCHG page and [atomics.types.operations].
</details>

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
  - 2 us gap: the atomic worker's CPU per ring was the whole gap (about 2.0 us, 100%) in every round, so it never slept. It is fast because it burns a whole core. cv used well under half of the gap.
  - 20 us and 200 us gaps: the atomic worker used more CPU per ring than cv in every round, at least 1.7x as much. The exact microseconds changed from round to round with load, so they are not given here.
  - Wake delay: this key makes no claim about which primitive woke sooner. Across the rounds recorded here the answer changed from round to round, so one run cannot settle it. If it matters to you, run several rounds and report the spread with the load average.
  - Reading: once the gap is longer than its spin window, the atomic waiter spins, then sleeps anyway, and pays both. This fits the shipped libc++ code (polls until about 4 us, then parks; see card M3-PB5), but the run does not show the mechanism directly.
  - Lesson: report CPU cost and wake counts next to latency, and sweep the gap. A spin-then-park primitive measured only at a tiny gap shows its spin, not its park.
  - Holds for / re-check: libc++ on macOS, no pinning (the OS decides where both threads run). Another library (libstdc++ on Linux) has different spin and park code, so rerun there, and after toolchain upgrades. A libstdc++ 13 rerun (3 rounds) on Ubuntu 24.04 on arm64 in a virtual machine on a busy host (GCC 13.3 / clang 18.1.3, libstdc++/glibc) went the same direction: at 20 and 200 us the atomic worker used about 2x the cv CPU per ring.
</details>

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

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
