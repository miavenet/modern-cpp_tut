# Lesson 06 — Practice Bank, part 9 of 11 (optional): Counters and Pipeline Models

**Time: about 30 minutes (estimate)** for 2 cards (15 + 15 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: raw perf events on another CPU, and store-to-load forwarding in llvm-mca.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB21 · Recall
Q: A runbook from another team says: "measure last-level cache misses with `perf stat -e r412e ./app`". You are about to run it on a CPU from a different vendor than the runbook author's. (a) What does `r412e` mean to `perf`, and why is it not safe to assume it counts the same thing on your CPU? (b) Name two primary sources you would check. (c) What vendor-neutral `perf` event would you ask for instead, and what do you lose by using it?
   Answer format: (a), (b), (c), then the habit.
<details><summary>Answer</summary>
Key:

  - (a) `r<hex>` is a raw event: `perf` does not translate it. `perf -vv` shows the event opened with `type 4` (the value of `PERF_TYPE_RAW`, "a raw implementation-specific event", in the kernel header) and `config 0x412e`, the same number you typed. For this Intel architectural event, event select is 2EH and unit mask 41H (`412e`: umask high byte, event low byte); Intel's manual lists that pair as "Last Level Cache Misses". Other x86 PMUs can have extended fields: perf_event_open(2) says some processors, like those from AMD, support event codes and unit masks larger than a byte. The meaning of a raw code belongs to the CPU vendor and model, so on another vendor's chip the same number can be a different event or an invalid one. On an Arm CPU (in a Linux container) `perf stat -vv` reported `type 4 (cpu)`, `config 0x412e`: `perf` accepts the number whether or not it means anything there.
  - (b) The CPU vendor's manual for your model (event name, code, exact definition) and the `perf-list(1)` / `perf_event_open(2)` man pages. `perf-list(1)` says to refer to "the processor specific documentation".
  - (c) The generalized event `cache-misses` (`PERF_COUNT_HW_CACHE_MISSES`, under `PERF_TYPE_HARDWARE`). `perf stat -e cache-misses -vv true` reported exactly that type and config. What you lose: the man page says it "usually" indicates last-level cache misses. The kernel chooses the underlying event per CPU, so the exact definition can differ between vendors. Intel's own manual warns that comparing values of its LLC event across implementations to estimate performance differences is not recommended.
  - Habit: when you copy a counter-based result, record the event your tool actually programmed (`-vv`), not only the friendly name or the runbook's raw code.
  - Holds for / re-check: Intel SDM (architectural performance events) and the man7.org pages as opened on 2026-11-16; `perf` 6.8.12 in an Ubuntu 24.04 aarch64 container. Counting itself was refused there (no permission), so no counter value was read; only the programmed attributes were seen. Nothing is claimed about which raw codes any non-Intel vendor uses. Re-check on your machine with `perf list` and `perf stat -e cache-misses -vv true`.
</details>

### Card M7-PB22 · Apply
Q: A loop stores its running counter to memory and reads it back each iteration: `str x1,[x0]; ldr x2,[x0]; add x1,x2,#1` (`mca/stfwd.s`). The same chain kept in a register is `add x1,x1,#1` (`mca/regchain.s`). Predict `llvm-mca`'s Total Cycles for 1000 iterations of each, with the default options and with `--noalias=false`:
   ```sh
   for f in regchain stfwd; do for o in "" --noalias=false; do llvm-mca -mtriple=aarch64 -mcpu=apple-m1 -iterations=1000 $o modules/07-architecture/lab/practice/mca/$f.s | grep "Total Cycles"; done; done
   ```
   What does the difference say about trusting the model on loops that touch memory?
   Answer format: four numbers, then what to check before reading a prediction as a speedup.
<details><summary>Answer</summary>
Key:

  - Model output, LLVM 22.1.6 `apple-m1`: `regchain` 1003 cycles in both modes. `stfwd`: 1006 by default (the memory chain costs the same as the register chain, as if the store and load never touch the same address) and 9003 with `--noalias=false` (about 9 cycles per iteration: the model now orders the load after the store).
  - LLVM 18.1.3 `neoverse-v2` in a Linux container: `stfwd` 673 by default and 6003 with `--noalias=false`; `regchain` 1003 in both. The direction (default much cheaper) is the same on both versions; the figures belong to each model.
  - So the model's answer for loops with store-to-load traffic (spills, counters in memory, aliasing pointers) depends on an alias assumption you must check, via `llvm-mca --help` (`--noalias`: "If set, assume that loads and stores do not alias"), before reading a prediction as a speedup. Measure on hardware, or re-run with the pessimistic setting and see how far the answer moves.
  - Holds for / re-check: model outputs only (deterministic per LLVM version and `-mcpu`; Homebrew LLVM 22.1.6 here); nothing was timed on hardware, and no claim is made about how real cores forward stores. The default behaviour above is what these two versions printed; re-run after an LLVM upgrade.
</details>

---

## Close

Next: part 10, `lesson-06-practice-bank-10-sanitizers-ub.md` (about 29 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
