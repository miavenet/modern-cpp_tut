# Lesson 06 — Practice Bank (optional): Atomics on Real Hardware, Waiting and Waking

**Time: about 135 minutes** for 14 cards (12 + 4 + 20 + 6 + 6 + 10 + 10 + 10 + 12 + 8 + 15 + 10 + 8 + 4 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: an atomic wait that never sleeps at a 2 us gap because it burns a whole core, and an acquire load that compiles to the same instruction as seq_cst on three of the four ARM targets tried here.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

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

### Card M3-PB6 · Apply
Q: `xchg_orders.cpp` has `f.exchange(true, order)` for relaxed, acquire and seq_cst, plus a release and a seq_cst `store(false)` and a relaxed load. Predict the instruction each becomes on (1) Apple arm64, (2) arm64 without LSE, the ARMv8.1 atomic instructions such as `swp` (`-mcpu=generic`), (3) x86-64. Then compile:
   ```sh
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   clang++ -std=c++20 -O2 -mcpu=generic -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/03-concurrency/lab/practice/xchg_orders.cpp -o -
   ```
   For each target, which operations (the exchanges, the stores) change instruction when only the memory order changes, and why does that matter when you port a lock benchmark?
   Answer format: one line per target, then why it matters.
<details><summary>Answer</summary>
Key:

  - arm64 (default CPU, LSE): relaxed `swpb`, acquire `swpab`, seq_cst `swpalb`; both stores `stlrb`; relaxed load `ldrb`.
  - arm64 `-mcpu=generic`: a load-exclusive/store-exclusive loop (`ldxrb/stxrb`, `ldaxrb/stxrb`, `ldaxrb/stlxrb`).
  - x86-64: all three exchanges are `xchgb`; the release store is a plain `movb`, the seq_cst store is `xchgb`; the relaxed load is `movzbl`. On x86 an exchange with a memory operand is always locked, whatever order you ask for (Intel SDM, XCHG). GCC 13.3 x86-64 (Compiler Explorer, `-O2`) shows the same pattern.
  - So the order changes different operations on different targets: on arm64 it changes the exchange while both stores are `stlrb`; on x86-64 it changes the store (`movb` vs `xchgb`) while every exchange is `xchgb`. A weak order on the exchange is invisible in the x86 code, so a lock with a too-weak order can look correct there. Re-test when moving between ISAs.
  - Not shown: the locked `xchg` fixes the hardware order only. The standard still lets the compiler move other accesses around a relaxed exchange; GCC 13.3 did not reorder a two-store test function, so that is a rule of the language, not an observation here.
  - GCC 13 on Ubuntu aarch64 emits a call to a helper (`bl __aarch64_swp1_acq`), not an inline instruction: the toolchain default matters too.
  - Holds for / re-check: Apple clang 21.0.0; GCC 13.3 on Ubuntu aarch64 (outline-atomics default); x86-64 is compile only, nothing x86 was run. Different CPU flags (`-mcpu`, `-march`) change the arm64 output; re-check with `-S`.
</details>

### Card M3-PB7 · Apply
Q: `ttas_lock.cpp` is a test-and-test-and-set (TTAS) lock guarding a plain `long counter`: it spins on a plain atomic load and tries the exchange only when the lock looks free. 4 threads add 20,000 each and the program checks counter == 80,000. Build it two ways: correct orders (`-DOK`: acquire exchange, release store) and all-relaxed (`-DRELAXED`). Run each 5 times at `-O2`, then 3 times under ThreadSanitizer:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DOK modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   clang++ -std=c++20 -O2 -Wall -Wextra -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   clang++ -std=c++20 -O1 -g -fsanitize=thread -DRELAXED modules/03-concurrency/lab/practice/ttas_lock.cpp -o "${TMPDIR:-/tmp}/ttas_lock" && "${TMPDIR:-/tmp}/ttas_lock"
   ```
   Is the counter check enough to accept the lock?
   Answer format: one line per build, then your verdict on the check.
<details><summary>Answer</summary>
Key:

  - Correct orders: PASS in all 5 plain runs; TSan reports 0 data races in 3 of 3 runs.
  - All-relaxed: the plain build FAILed in some runs and PASSed in others (for example 3 FAIL, 2 PASS out of 5). TSan reported a data race on `counter` in 3 of 3 runs (exit 134).
  - So a passing invariant check is necessary but not proof: a wrong lock can pass on a given run or machine. Add a race detector run, which checks the ordering rule, not one outcome.
  - Relaxed exchange/store do not create a happens-before edge between the unlock and the next lock, so the plain `counter` accesses race.
  - Holds for / re-check: Apple clang 21.0.0 on M1 Max (a weakly ordered CPU). How often the relaxed build fails depends on the CPU and timing; on x86 the exchange is always locked (card M3-PB6), so it may fail less often or not at all there (not run here; the compiler may still reorder relaxed code). The TSan result does not depend on the CPU. Re-check: rerun both builds on the target box.
</details>

### Card M3-PB8 · Apply
Q: You want a 16-byte `{pointer, tag}` pair updated with one compare-and-swap, and a teammate says "`std::atomic<Tagged>` is lock-free on 64-bit machines." `wide_cas.cpp` prints `is_always_lock_free` and `is_lock_free()` for a 16-byte struct and contains one `compare_exchange_strong`. Check (a) the printed values with each compiler you have, and (b) which instruction or call the CAS becomes in the listing for each target:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos13 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos13 -march=x86-64 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   ```
   (Then add `-mcx16` to the last one. GCC on Linux needs `-latomic` to link.) What do you find?
   Answer format: one line per toolchain or target, then what a `static_assert` on `is_always_lock_free` proves.
<details><summary>Answer</summary>
Key:

  - Apple clang 21, arm64 native: `is_always_lock_free` = 1, `is_lock_free()` = 1; the CAS is one `caspal` instruction.
  - Apple clang 21, `-target x86_64-apple-macos13`: `lock cmpxchg16b`. With `-march=x86-64` (a baseline without the 16-byte CAS feature) the same code becomes a call to `___atomic_compare_exchange_16`; adding `-mcx16` brings `lock cmpxchg16b` back.
  - GCC 13.3, aarch64 Linux (needs `-latomic`): `is_always_lock_free` = 0, `is_lock_free()` = 0, and the CAS is a call to `__atomic_compare_exchange_16`.
  - clang 18.1.3 with libstdc++ on the same Linux, default flags: `is_always_lock_free` = 1 but `is_lock_free()` = 0. The listing has no libatomic call for the CAS: it calls `__aarch64_cas16_acq_rel` (a helper that picks the instruction at run time), while `is_lock_free()` calls `__atomic_is_lock_free`. With `-march=armv8.1-a` the CAS is an inline `caspal`, and `is_lock_free()` still prints 0.
  - Per [atomics.types.operations] p4 and p5 (both normative), `is_always_lock_free` is true only if the type's operations are always lock-free, and `is_lock_free()` returns true if they are lock-free; Note 3 says the two are consistent. So the clang 18 + libstdc++ pair (1 and 0) cannot both be right: one of the two answers is wrong for that toolchain.
  - So `static_assert(std::atomic<T>::is_always_lock_free)` is necessary but not sufficient: here it passes while `is_lock_free()` says 0. Check both on the target, and read the listing (is the CAS an inline `cmpxchg16b`/`casp` or a call?).
  - Holds for / re-check: the toolchain/target combinations named above (Linux: Ubuntu 24.04 aarch64); x86 results come from Apple clang cross-compiling, not from an x86 run. They change with `-march`/`-mcpu`, library version, and compiler release. Re-check with `wide_cas.cpp` (`-DMAIN` to print) and `-S` on every new toolchain.
</details>

### Card M3-PB9 · Apply
Q: You must copy 64 bytes from a live, correctly aligned source into a separate private destination. Another thread writes the source, using `std::atomic_ref` stores of the same type and granularity as each reader below, with no concurrent plain or differently sized access to those bytes. Three versions in `atomic_copies.cpp`: `copy_plain` (`memcpy`, a data race if a write overlaps), `copy_bytes` (each byte through `std::atomic_ref<unsigned char>` relaxed), `copy_words` (eight `std::atomic_ref<std::uint64_t>` relaxed loads on a `uint64_t[8]` struct). Predict how many load instructions each one executes, then check here and with x86-64 GCC and clang on Compiler Explorer:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_copies.cpp -o -
   ```
   Answer format: loads per version, then what the atomic versions do and do not give you.
<details><summary>Answer</summary>
Key:

  - `copy_plain`: wide vector loads. arm64: two `ldp q` load instructions (each loads two 16-byte registers, four 16-byte chunks in all); x86-64: four 16-byte loads (GCC `movdqu`, clang `movups`).
  - `copy_bytes`: 64 single-byte loads at run time. arm64 and x86 GCC: a loop with one `ldrb`/`movzbl` per iteration; x86 clang: the same loop unrolled by 4. No compiler merged the atomic byte loads.
  - `copy_words`: eight 8-byte loads executed (arm64 `ldr x`, unrolled; x86 clang `movq`, unrolled; x86 GCC `movq`, in an 8-iteration loop).
  - Lesson: atomic loads make each individual access race-free, and the listing shows their price; word-sized relaxed atomics cost far fewer loads than bytewise. This does not give a consistent snapshot: the copy can still mix values from different writes, so a seqlock-style check is still needed. No speedup is shown here, only instruction counts; measure before choosing, and check the listing again when the payload size changes.
  - Holds for / re-check: Apple clang 21 arm64, GCC 13.3 and clang 18.1 x86-64 (Compiler Explorer), `-O2`. `atomic_ref` needs the object alive, suitably aligned (`required_alignment`) and accessed only through `atomic_ref` while references exist; no subobject may be referenced concurrently by another `atomic_ref` ([atomics.ref.generic] p3, C++23 draft N4950), so mixing the byte and word versions on the same memory would be wrong. A future standard atomic memcpy, or a compiler that merges adjacent relaxed loads, would change this.
</details>

### Card M3-PB10 · Spot the trap
Q: In `volatile_flag.cpp` one thread fills a plain `Config` struct and then sets `volatile bool g_ready = true`; another spins `while (!g_ready) {}` and prints the struct. It prints the right values on your laptop. What is wrong, and which tool shows it without needing a lucky failure?
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=thread modules/03-concurrency/lab/practice/volatile_flag.cpp -o "${TMPDIR:-/tmp}/volatile_flag" && "${TMPDIR:-/tmp}/volatile_flag"
   ```
   Answer format: what the tool reports, the rule, then the fix.
<details><summary>Answer</summary>
Key:

  - ThreadSanitizer reports two data races: on the flag (the spin line) and on the payload (the print line).
  - `volatile` gives neither atomicity nor inter-thread ordering. A non-atomic write and a concurrent read with no happens-before between them is a data race, which is undefined behaviour ([intro.races] p21, C++23 draft N4950).
  - "Prints the right values" proves nothing: correct output can be luck of the hardware and the compiler.
  - **Fix:** `std::atomic<bool>` with a release store and acquire load (or a mutex).
  - Holds for / re-check: any conforming compiler; the TSan report was seen with Apple clang 21 on arm64 macOS. TSan only finds races that the run executes, so a quiet run is not proof; a sanitizer can also miss races it does not model, such as fence-based code.
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

### Card M3-PB12 · Recall
Q: What is `lock_shared()` built from in two standard libraries, and how do you find out? (a) Compile a function that takes `std::shared_lock` and read the calls in the `-S` output; (b) read the class in the library header; (c) look at the function body that the program really runs (a debugger `disassemble` on a running process works). What did you find for libc++ (Apple clang) and for libstdc++ (GCC 13) on Linux?
   ```sh
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/reader_lock.cpp -o -
   ```
   Answer format: one line per library, then the technique.
<details><summary>Answer</summary>
Key:

  - libc++ (Apple clang 21): `read_value()` calls `std::__shared_mutex_base::lock_shared` / `unlock_shared`. The header shows the base holds a `mutex`, two `condition_variable`s and an `unsigned` state. The shipped `lock_shared` body (disassembled with lldb in a running process) calls `std::mutex::lock()`, loads the state word, stores it back incremented, then calls `std::mutex::unlock()`. Upstream libc++ `llvmorg-22.1.6` `src/shared_mutex.cpp` (byte-identical to `llvmorg-21.1.0`) has the same structure (`unique_lock lk(__mut_)`, `__state_` incremented); the upstream file is corroboration, not proof of which source Apple built.
  - libstdc++ (GCC 13.3, Ubuntu 24.04 aarch64, glibc 2.39): the calls are `pthread_rwlock_rdlock` and `pthread_rwlock_unlock` (header line `return pthread_rwlock_rdlock(__rwlock)`). The glibc 2.39 `pthread_rwlock_rdlock` disassembled with gdb has its first call at +68, after two checks: `__aarch64_ldadd4_acq`, an atomic add on the lock word at its fast path, in line with `nptl/pthread_rwlock_common.c` (glibc 2.39).
  - Technique: the standard says what a class does, not how; the listing, header and the body that runs say how. Check all three on each target library.
  - Holds for / re-check: SDK libc++ `_LIBCPP_VERSION 220106` with Apple clang 21 on macOS 27.0.1 (lldb disassembly of the shipped `libc++.1.dylib`), libstdc++ of GCC 13.3 with glibc 2.39 on Linux aarch64. Do not generalise to all standard libraries: implementations change between releases, so re-check with `-S`, the header and a disassembly for each new toolchain.
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

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
