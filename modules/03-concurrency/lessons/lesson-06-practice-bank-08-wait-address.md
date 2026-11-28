# Lesson 06 — Practice Bank, part 8 of 8 (optional): The Word That Actually Waits

**Time: about 10 minutes (estimate)** for 1 card (10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: matching the address a Linux futex waits on to the atomic object, by element type, beyond reading the wait implementation (part 3).

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

Terms. *Futex*: a Linux kernel waiting operation on a memory word. *Proxy word*: a different word the library waits on for the object.

## Cards

### Card M3-PB19 · Apply
Q: (Linux only: run it on Linux or in a Linux container.) A worker waits for a one-shot completion value to change from 0 to 7. The producer gives it a chance to park, then stores 7 and notifies it. `wait_address.cpp` builds the identical protocol with `atomic<int>` and `atomic<char>`. Predict whether a futex wait uses the address printed as `OBJECT` in each build. Trace the syscall entry addresses, classify WAIT separately from WAKE, and explain why a wake call alone does not prove that a thread parked.
   ```sh
   bash modules/03-concurrency/lab/practice/wait_address.sh
   ```
   The script needs `g++`, `strace` and `python3`. On a Mac, run it in a Linux container from the tutorial root: `docker run --rm --network none -v "$PWD":/work -w /work <image> bash modules/03-concurrency/lab/practice/wait_address.sh`, with an image that has those three tools (the answer used Ubuntu 24.04). It builds both variants with GCC, saves each run's futex trace and stdout/stderr in a folder in your temporary directory, and prints the installed header's lines for `__platform_wait_uses_type` and `_S_wait_addr`. Its census (`wait_census.py`) ignores resumed syscall lines and separates plain WAIT from the thread-join WAIT_BITSET. It also requires a successful WAIT completion and a WAKE that found a waiter on the same address. If either is missing, report an inconclusive parking probe and repeat; the 50 ms sleep is not proof of scheduling.
   Answer format: for int and char, whether the plain WAIT address equals `OBJECT` and whether a WAKE hit it; what the header selects; then why a WAKE alone proves nothing.
<details><summary>Answer</summary>
Key:

  - This libstdc++ build: `int` is 4 bytes and its plain WAIT address equals `OBJECT`; `char` is 1 byte and its plain WAIT address differs. Both workers print `observed=7`.
  - The saved traces each contain one plain WAIT entry. The int has one WAKE on the object's address; the char has none there. Grade address identity and observed parking, not total futex calls, process IDs or literal addresses.
  - The installed `bits/atomic_wait.h` selects the object's address when its type meets `__platform_wait_uses_type`; otherwise `_S_wait_addr` selects the waiter-pool version word. The trace corroborates the selected path. Type size alone cannot identify the syscall address without that check.
  - A WAKE syscall may find no sleeper. Match a WAIT entry and its completion when claiming parking; unrelated runtime and join calls also appear in the trace.
  - Holds for / re-check: Linux only: GCC 13.3, libstdc++ `__GLIBCXX__ 20240904` (printed by the script), Ubuntu 24.04 aarch64 container, kernel 7.0.12-linuxkit, C++20 `-O2 -Wall -Wextra -pthread`. Linux implementation evidence, not an ISO requirement about syscalls; no latency or macOS kernel-call claim. Scheduling may prevent parking in a particular run; an absent WAIT is inconclusive, not a contrary address result. Re-run the script with the shipping library, record the object address anew, and revisit the header after upgrades.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
