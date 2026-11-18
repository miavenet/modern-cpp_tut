# Lesson 07 — Practice Bank (optional): What Memory Really Costs, What the Compiler Assumes

**Time: about 50 minutes** for 7 cards (6 + 6 + 10 + 10 + 10 + 3 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: an 8-byte value that takes a 32-byte block in a `std::list`, and a range mask the compiler deleted because you promised it was not needed.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++); counts and listings are steadier than timings.

## Cards

### Card M5-PB1 · Spot the trap
Q: A function contains `[[assume(i >= 0 && i < 8)]];` and then reads `table[i & 7]` from an 8-entry table. Someone calls it with 9 and says "the mask keeps it in range, and it printed a value, so it is fine". Build it with no sanitizer at -O0, -O1 and -O2, and run each binary with 3 and with 9. Predict first: will all three builds print the same thing for 9?
   ```sh
   for o in 0 1 2; do clang++ -std=c++23 -O$o modules/05-memory-perf/lab/practice/assume_mask.cpp -o "${TMPDIR:-/tmp}/assume$o" && "${TMPDIR:-/tmp}/assume$o" 3 && "${TMPDIR:-/tmp}/assume$o" 9; done
   ```
   Answer format: what each build prints for 9, then why, in one line.
<details><summary>Answer</summary>
Key:

  - With 3 every build prints `lookup(3)=4`.
  - With 9 the builds disagree: `lookup(9)=2` at -O0 (9 & 7 = 1, so `table[1]`), and `lookup(9)=0` at -O1 and -O2 (same in 3 runs of each).
  - Why: a false assumption is undefined behaviour ([dcl.attr.assume] p1 in C++23, N4950; the 2026 drafts word it as runtime-undefined behaviour). It is a precondition, not a comment. At -O2 the compiler used it: the `and` with 7 is gone from the assembly, so the load reads past the end of the table. The "safe" mask was removed because you promised it was not needed.
  - After undefined behaviour, the printed value is not a fact about your code. Different -O levels giving different answers is a strong hint that you have UB.
  - Fix: guarantee the precondition at the caller, or check it and handle the error, before assuming it.
  - Holds for / re-check: the values printed after the UB are specific to Apple clang 21 on arm64 and can change with any compiler, flag or layout change; the rule that a false assumption is UB is in the C++23 and C++26 drafts. A compiler that does not know the attribute ignores it: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), clang 18 warned `unknown attribute 'assume' ignored`, kept the mask and printed 2 at every level; GCC 13 printed 0 at -O1 and -O2. Re-check by building at several -O levels and reading the assembly of the function.
</details>

### Card M5-PB2 · Spot the trap
Q: A service uses one `std::pmr::monotonic_buffer_resource` per session. Its hot loop allocates a 64-byte scratch object and deallocates it each iteration, "so memory use stays flat". A session lasts hours. What happens, and what does the standard say?
   Answer format: what happens, the rule, and one fix.
<details><summary>Answer</summary>
Key:

  - `do_deallocate` on a monotonic resource has "Effects: None", and "Memory used by this resource increases monotonically until its destruction." So the loop grows memory without bound.
  - Measured here with a counting upstream: after 100k, 200k and 300k alloc/free pairs, upstream bytes were about 8.4 MB, 16.8 MB and 33.6 MB (buffers grow geometrically).
  - Fix: call `release()` at a safe point (for example per message batch), use a fresh resource per unit of work, or use a pool resource (`unsynchronized_pool_resource`) when blocks are really freed and reused.
  - Holds for / re-check: the no-op deallocate is required by the standard, so it holds on every implementation; the growth sizes are libc++'s choice and may differ elsewhere: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ 13 gave about 6.8, 15.3 and 23.0 MB at the same points. Re-check the growth with `clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/pmr_monotonic.cpp -o "${TMPDIR:-/tmp}/pmr_monotonic" && "${TMPDIR:-/tmp}/pmr_monotonic"`.
</details>

### Card M5-PB3 · Apply
Q: A code review comment says "use `x & (n-1)` instead of `x % n` for a power-of-two `n`, it is cheaper". You want to know if that holds for your compiler and CPU. Describe the check, say what you would count, and predict the result for four cases: `x` unsigned or signed, `n` a compile-time constant or a runtime value. Then run the check:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/modmask.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/modmask.cpp modules/05-memory-perf/lab/practice/modmask_neg.cpp -o "${TMPDIR:-/tmp}/modmask_neg" && "${TMPDIR:-/tmp}/modmask_neg"
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/layout_ldp.cpp -o -
   ```
   Answer format: the method in one line, then one line per case.
<details><summary>Answer</summary>
Key:

  - Write each case as a tiny `extern "C"` function, compile with `-O2 -S`, and read the listing. Count the instructions on the path that runs, not the source lines.
  - Here (Apple clang 21, arm64): unsigned `x`, constant `n = 1024`: both compile to the same single `and x0, x0, #0x3ff`. The rewrite gains nothing; the compiler already does it.
  - Signed `x`, constant `n`: `x % 1024` becomes `negs`, two `and`, `csneg` (4 instructions) because the result must keep the sign of `x`. `x & 1023` is one `and`, but it gives a different answer for negative `x` (run here: `-5 % 1024` = -5, `-5 & 1023` = 1019). So the "optimisation" can change behaviour; make the type unsigned instead.
  - Unsigned `x`, runtime `n`: `x % n` is `udiv` + `msub`; `x & (n-1)` is `sub` + `and`. The mask avoids the divide instruction; time it before claiming a speed-up. It is only correct if the caller guarantees `n` is a power of two.
  - Signed `x`, runtime `n`: `x % n` is `sdiv` + `msub`; `x & (n-1)` is `sub` + `and`. Same trade as the unsigned case, plus the sign problem: the results differ for negative `x` (run here with `n` passed at run time: -5 vs 1019).
  - Counts also shift with unrelated details. Example: two loads of adjacent struct fields became one `ldp` here; moving one field 16 bytes away gave two `ldr`. Compare whole functions in their real context before believing a one-instruction claim. Then time it in a loop if it matters.
  - Holds for / re-check: these listings are specific to this compiler version, arm64 and `-O2`. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 gave the same four shapes; x86-64 was not checked. Repeat the four cases there, and re-read the listing after compiler upgrades.
</details>

### Card M5-PB4 · Apply
Q: A record is `struct Rec { uint32_t id; uint64_t qty; };`, so the raw data is 12 bytes. Predict the heap bytes per element for `std::list<Rec>`, `std::set<Rec>`, `std::deque<Rec>`, and `std::vector<Rec>` with and without `reserve(n)`. Then run the program, which counts bytes requested by each container and bytes malloc actually granted (`malloc_size`):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/bytes_per_elem.cpp -o "${TMPDIR:-/tmp}/bytes_per_elem" && "${TMPDIR:-/tmp}/bytes_per_elem"
   ```
   Answer format: `sizeof(Rec)`, then bytes per element for each container.
<details><summary>Answer</summary>
Key:

  - Padding first: `sizeof(Rec)` is 16, not 12 (`alignof` 8): the 8-byte field must be 8-aligned. With only these two fields, swapping their order cannot help: the size must be a multiple of the 8-byte alignment, and 12 rounds up to 16.
  - Requested, here (libc++), at n = 1000, 50000 and 1000000: list 32 (node = prev and next pointers + Rec), set 48 (node = left, right and parent pointers and a colour flag, padded, + Rec), deque 16.04-16.42, vector after reserve 16.00.
  - Vector without reserve keeps its growth slack: 16.38, 20.97, 16.78 bytes/element at the three sizes, depending on how far n is from the last capacity step.
  - Granted vs requested, macOS malloc: rounding showed up only for the vector after reserve (16.38, 16.06, 16.01 granted vs 16.00 requested); the node containers matched exactly. `malloc_size` does not include malloc's own bookkeeping, so it is still not the full process cost.
  - These are counts, not timings: two runs gave identical numbers.
  - Holds for / re-check: libc++ node layouts and growth policy, macOS malloc size classes, Apple clang 21, `-O2`. Other standard libraries and allocators differ: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), the list, set and vector requests were the same, deque requested 16.41-17.01, and glibc rounded the nodes too (list granted 40, set 56). `bytes_per_elem.cpp` builds on macOS only (`malloc_size` is a macOS call); the Linux numbers come from `bytes_per_elem_glibc.cpp` in the same folder, the same program with glibc's `malloc_usable_size`: `g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/bytes_per_elem_glibc.cpp -o "${TMPDIR:-/tmp}/bytes_per_elem_glibc" && "${TMPDIR:-/tmp}/bytes_per_elem_glibc"`. Re-check by running the matching program on the target.
</details>

### Card M5-PB5 · Apply
Q: A colleague estimates the memory of a node-based container as `N * sizeof(value_type)`. How do you measure the real bytes per element on your machine, in two steps? Then run this program, which does both steps for three containers, and read what each step shows:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes.cpp -o "${TMPDIR:-/tmp}/node_bytes" && "${TMPDIR:-/tmp}/node_bytes"
   ```
   Answer format: step 1 and step 2, one line each, with what each showed in your run.
<details><summary>Answer</summary>
Key:

  - Step 1: count what the container asks the allocator for, with a counting allocator. You do not need to guess the node layout. Here a `std::list` node for a 40-byte value asks for 56 bytes; a `std::map<int, 40-byte value>` node asks for 80; a `std::list<std::uint64_t>` node asks for 24.
  - Step 2: ask the allocator how big each live node's block really is. Here `malloc_size` on the node's own pointer gives 64 for the 56-byte request, 80 for 80, and 32 for 24.
  - So the 8-byte value really takes a 32-byte block per element here, 4 times the estimate (not counting the allocator's own bookkeeping outside the block, which this test does not see).
  - Holds for / re-check: node layout is a library detail (here libc++ 220106); rounding is allocator-specific, and `malloc_size` is a macOS call. Other allocators have their own size query, but it may not measure the same thing (for example, it may leave out a per-block header). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), glibc's `malloc_usable_size` gave 24 for the 24-byte node, but consecutive blocks sat 32 bytes apart: it leaves out the 8-byte header. Read its documentation, or measure memory growth over many nodes instead. `node_bytes.cpp` builds on macOS only; the Linux numbers come from `node_bytes_glibc.cpp` in the same folder (the same steps with `malloc_usable_size`, plus the address step between consecutive blocks): `g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes_glibc.cpp -o "${TMPDIR:-/tmp}/node_bytes_glibc" && "${TMPDIR:-/tmp}/node_bytes_glibc"`. Re-check by running the matching program on each library and allocator.
</details>

### Card M5-PB6 · Recall
Q: Intel's optimization manual describes "4K aliasing" (first attempt: look it up in the Intel 64 and IA-32 Architectures Optimization Reference Manual 248966-051, section 15.8; on reviews, recall it). In one sentence: what pattern triggers it, how big is the stated penalty, and which performance event counts it?
   Answer format: one sentence.
<details><summary>Answer</summary>
Key:

  - A load shortly after a store to a different address whose address bits 5-11 match (a 4 KiB offset), with overlapping byte offsets.
  - The manual says it "may have a five-cycle penalty" on load latency.
  - Event: `LD_BLOCKS_PARTIAL.ADDRESS_ALIAS`.
  - Holds for / re-check: stated in the AVX optimization chapter of manual 248966-051 (Haswell-era wording); the penalty and event can differ on newer cores; check that core's event list. AMD and Apple cores have their own rules; not checked.
</details>

### Card M5-PB7 · Spot the trap
Q: Someone pads hot atomics with `alignas(std::hardware_destructive_interference_size)` and assumes that constant equals the cache line size. Print it on your machine and compare with `sysctl hw.cachelinesize` (macOS; on Linux try `getconf LEVEL1_DCACHE_LINESIZE`, which printed 64 in an Ubuntu 24.04 arm64 VM). What do you see, and what does the standard actually promise?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_size.cpp -o "${TMPDIR:-/tmp}/line_size" && "${TMPDIR:-/tmp}/line_size" && sysctl hw.cachelinesize
   ```
   Answer format: the three numbers, then the promise in one line.
<details><summary>Answer</summary>
Key:

  - Here: `hardware_destructive_interference_size` = 256, `hardware_constructive_interference_size` = 64, `sysctl hw.cachelinesize` = 128. None of the three numbers is the same.
  - The standard only says the destructive value is "the minimum recommended offset between two concurrently-accessed objects to avoid additional performance degradation due to contention", at least `alignof(max_align_t)`, and implementation-defined. It is not defined as the line size.
  - It is a compile-time constant, so one binary uses the same value on every CPU it runs on. Measure false sharing on the target, or use the OS-reported line size.
  - Holds for / re-check: values are from Apple clang 21 / libc++ on arm64-apple-darwin; other libraries and targets may choose other values or none: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 also gave 256/64, while clang 18 with libstdc++ did not define the constants at all. Re-check by printing both constants after each toolchain change.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
