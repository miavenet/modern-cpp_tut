# Lesson 06 — Practice Bank, part 10 of 12 (optional): Prefetching and the Manuals

**Time: about 24 minutes (estimate)** for 3 cards (5 + 4 + 15 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what a prefetch guarantees, when it pays off, and what a VM exit costs.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

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

---

## Close

Next: part 11, `lesson-06-practice-bank-11-code-shape.md` (about 24 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
