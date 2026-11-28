# Lesson 06 — Practice Bank, part 6 of 8 (optional): Fences on Four CPUs

**Time: about 27 minutes (estimate)** for 2 cards (15 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what thread fences and ordered loads and stores compile to on x86-64, arm64, POWER and RISC-V, extending the instruction table in lesson 02, CONCEPT C5; and why a fence needs an atomic operation to link it to the other thread.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

Terms. *Fence*: an operation that orders memory accesses around it. *Thread fence*: `std::atomic_thread_fence`, which orders against other threads.

## Cards

### Card M3-PB16 · Apply
Q: `fence_orders.cpp` holds six one-line functions: acquire, release and seq_cst thread fences; an acquire load; a release store; a seq_cst store. Before compiling, fill a 6-row table for x86-64 and arm64: "no instruction" or "an instruction". Then fill it from real listings: on your machine with
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/fence_orders.cpp -o -
   ```
   and on Compiler Explorer (godbolt.org) for x86-64 (GCC 13.3 and clang 18.1), POWER (GCC 13.2) and RISC-V (GCC 13.3), each with `-O2 -std=c++20`.
   Answer format: the six rows per target, then one sentence on what an empty row does and does not allow.
<details><summary>Answer</summary>
Key:

  - x86-64 (GCC 13.3 and clang 18.1, Compiler Explorer listings): acquire and release thread fences, acquire load and release store emit no extra CPU instruction (`ret` only, or a plain `movl`). The seq_cst store is `xchgl` on both. The seq_cst thread fence differs by compiler: GCC `lock orq $0,(%rsp)`, clang `mfence`.
  - arm64 (Apple clang 21, compiled here): acquire fence `dmb ishld`; release and seq_cst fences `dmb ish`; acquire load `ldapr`; release and seq_cst stores `stlr`.
  - POWER (GCC 13.2) and RISC-V (GCC 13.3), Compiler Explorer listings: POWER acquire and release fences `lwsync`, seq_cst fence `sync`; RISC-V `fence r,rw`, `fence rw,w`, `fence rw,rw`.
  - Lesson: no instruction in a listing is not permission to drop the C++ synchronization ([intro.races] p21, C++23 draft N4950). Use the atomic operations the memory model requires; a platform extension needs its own explicit contract. Two compilers can pick different instructions for the same order on the same CPU.
  - Holds for / re-check: these scalar atomics in ordinary memory, with the compilers and flags named. The x86-64, POWER and RISC-V lines come from Compiler Explorer listings, not runs: re-check by pasting `fence_orders.cpp` into Compiler Explorer with the same compiler and `-O2 -std=c++20`. Lowering can change with `-march`/`-mcpu` (for example arm64 `ldapr` needs a recent baseline) and with compiler releases. No claim is made about hardware ordering beyond what these listings show.
</details>

### Card M3-PB17 · Spot the trap
Q: In a fresh one-shot publication, both atomics start at 0. One writer stores `payload=83` relaxed, executes a release thread fence A, then stores `token=1` relaxed (X). The reader loads `token` relaxed (Y); if it reads 1, it executes an acquire thread fence B and loads `payload` relaxed. Identify A/X/Y/B in `fence_witness.cpp`. Must an accepted read return 83? Now move B before Y (`receive_early`). Which standard proof fails? Compile all forms and decide whether counting barriers alone distinguishes the correct reader from the broken one. Run the correctly ordered protocol, then read the listing (on an arm64 machine; elsewhere the listing is for your own CPU):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DRUN modules/03-concurrency/lab/practice/fence_witness.cpp -o "${TMPDIR:-/tmp}/fence_witness" && "${TMPDIR:-/tmp}/fence_witness"
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/fence_witness.cpp -o -
   ```
   For x86-64, paste `fence_witness.cpp` into Compiler Explorer (godbolt.org) with x86-64 clang 20.1.0 and `-std=c++20 -O2 -Wall -Wextra`. Only the correctly ordered protocol is run; the other reader is compiled for inspection. All shared payload accesses are atomic; no intentionally racy plain-data program is run.
   Answer format: A/X/Y/B, whether an accepted read must return 83, the proof step that fails for `receive_early`, where each listing puts its barrier, then whether counting barriers is enough.
<details><summary>Answer</summary>
Key:

  - Correct reader: yes, conditional on Y reading X's 1. A precedes X, Y precedes B, and Y reads X's value on the same atomic `token`: [atomics.fences] p2 supplies the synchronization edge. The payload store happens before the final payload load, which must see 83 in this one-writer, one-publication case.
  - Early B: the required Y-before-B relation is absent. There is still an acquire barrier in the Arm listing, but its mere presence does not establish the needed edge. The C++ proof no longer guarantees 83.
  - Apple clang 21 arm64: the correct `receive` has `dmb ishld` after the token check; `receive_early` has it before the token load. The early variant passes a naive "one barrier exists" test.
  - x86-64 clang 20.1.0 (a Compiler Explorer listing, not a run): the two reader forms have the same instruction sequence here. Matching x86 listings cannot certify the C++ synchronization argument.
  - Holds for / re-check: C++20 probes, Apple clang 21.0.0 arm64 and x86-64 clang 20.1.0 on Compiler Explorer, `-O2 -Wall -Wextra`. No later writer, no token reuse and no other synchronization before the reader's loads. The run recorded zero stale accepted reads but cannot prove the protocol. The semantic proof is from [atomics.fences] p2, with payload coherence from [intro.races] p18 (write-read coherence), C++23 draft N4950; instruction choices are compiler/target observations. Re-run the commands and check barrier location as well as existence; rederive A/X/Y/B whenever the protocol changes.
</details>

---

## Close

Next: part 7, `lesson-06-practice-bank-07-cas-equality.md` (about 8 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
