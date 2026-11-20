# Lesson 07 — Practice Bank (optional): What Memory Really Costs, What the Compiler Assumes

**Time: about 225 minutes** for 27 cards (6 + 6 + 10 + 10 + 10 + 3 + 5 + 8 + 8 + 8 + 6 + 6 + 12 + 8 + 6 + 4 + 12 + 10 + 10 + 12 + 5 + 8 + 12 + 12 + 12 + 10 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: an 8-byte value that takes a 32-byte block in a `std::list`, and a range mask the compiler deleted because you promised it was not needed.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

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

### Card M5-PB8 · Apply
Q: `layout.cpp` has three versions of a struct with two `std::atomic<uint64_t>` counters, one written by each of two threads: `Stats` (plain), `StatsAlignedStruct` (`struct alignas(128) ...`), and `StatsAlignedMembers` (`alignas(128)` on each member). Predict the offset of the second counter and the `sizeof` for each. Then ask the compiler:
   ```sh
   clang++ -std=c++20 -Xclang -fdump-record-layouts -fsyntax-only modules/05-memory-perf/lab/practice/layout.cpp
   ```
   Answer format: offset, `sizeof` and alignment per struct, then the habit.
<details><summary>Answer</summary>
Key:

  - `Stats`: second counter at offset 8, sizeof 16, align 8.
  - `StatsAlignedStruct`: second counter still at offset 8; sizeof 128, align 128. Aligning the struct only moves where the struct starts and pads its end. Both counters are still on one line.
  - `StatsAlignedMembers`: second counter at offset 128, sizeof 256, align 128. Only this version puts the counters on different 128-byte lines.
  - Habit: read the offsets the compiler actually chose (the layout dump, or `offsetof` in a `static_assert`) instead of reasoning from the source. No load test is needed.
  - Holds for / re-check: `-fdump-record-layouts` is a Clang pass-through (`-Xclang`) flag, not a stable public option; it may change between releases. `offsetof` is only guaranteed for standard-layout classes ([support.types.layout] p1, C++23 draft N4950: use with other types is conditionally-supported); all three structs here are standard-layout (checked with `std::is_standard_layout_v` on Apple clang 21, GCC 13.3 and clang 18). Offsets above are for arm64 Apple clang 21; with `alignas` they are set by the code, not the target.
</details>

### Card M5-PB9 · Apply
Q: Add a compile-time guard so that a later edit cannot quietly put the two counters back on one line. First try the obvious `static_assert(offsetof(S, rejects) - offsetof(S, fills) >= 128)`. Then ask: what happens to it if someone swaps the two members so `rejects` comes first? Write a guard that is safe in both orders, and test it on the four structs in `layout_guard.cpp` (`AlignedStruct`, `AlignedMembers`, and the same two with the members swapped), one `-D` flag per struct; `OLD_GUARD_REORDERED` applies the subtraction guard to the swapped struct:
   ```sh
   for s in T_ALIGNED_STRUCT T_ALIGNED_MEMBERS T_REORDERED_STRUCT T_REORDERED_MEMBERS OLD_GUARD_REORDERED; do echo "== $s"; clang++ -std=c++20 -Wall -Wextra -D$s -fsyntax-only modules/05-memory-perf/lab/practice/layout_guard.cpp && echo compiles; done
   ```
   Answer format: what happens to the subtraction guard, your order-safe guard, then which structs compile.
<details><summary>Answer</summary>
Key:

  - `offsetof` is unsigned (`std::size_t`). With the members swapped, `rejects` is at 0 and `fills` at 8, so `0 - 8` wraps around to a huge number and the subtraction guard passes silently on a struct whose counters share a line. Apple clang 21, clang 18 and GCC 13.3 all accepted it (`-DOLD_GUARD_REORDERED`, exit 0).
  - Order-safe guard: compare the line index of each member, `offsetof(T, fills) / kLine != offsetof(T, rejects) / kLine`, and also require `alignof(T) % kLine == 0` (the struct must start on a line), plus `std::is_standard_layout_v<T>` for `offsetof`.
  - Results, same on all three compilers: `AlignedStruct` and its swapped version fail to compile (both counters in offsets 0-15, one line); `AlignedMembers` and its swapped version compile.
  - Habit: write the check so it fails for the bad layout in every member order, and test that it fails (a guard you have never seen fail proves nothing).
  - Holds for / re-check: `offsetof`, `static_assert` and `std::is_standard_layout_v` are standard C++; `offsetof` needs a standard-layout class ([support.types.layout] p1). Diagnostic wording differs between compilers (Apple clang 21 also prints the evaluated expression). The guard checks offsets against your constant `kLine`; it cannot tell you the hardware line size (card M5-PB10). Checked on Apple clang 21, and on GCC 13.3 / clang 18 on aarch64 Linux.
</details>

### Card M5-PB10 · Apply
Q: A teammate pads hot fields with `alignas(std::hardware_destructive_interference_size)` in a shared header and says "the standard library knows the right line size." Run `hdis.cpp` on each toolchain you have and compare with what the OS reports (`sysctl hw.cachelinesize` on macOS, `getconf LEVEL1_DCACHE_LINESIZE` on Linux, including inside a Linux container on the same Mac). Then build `use_hdr.cpp` with GCC and `-Wall`. What do you find?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/hdis.cpp -o "${TMPDIR:-/tmp}/hdis" && "${TMPDIR:-/tmp}/hdis"
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/05-memory-perf/lab/practice/hdis.cpp -o -
   g++ -std=c++20 -Wall -c modules/05-memory-perf/lab/practice/use_hdr.cpp -o "${TMPDIR:-/tmp}/use_hdr.o"
   ```
   Answer format: one line per toolchain or source, then what you put in your code instead.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 on arm64 gives destructive = 256 and constructive = 64. The Mac reports a 128-byte line. The constant is a compiler choice (a safe upper bound), not a reading of the hardware.
  - Apple clang 21 targeting x86-64 gives 64 for the same source.
  - GCC 13.3 on aarch64 gives 256 / 64. With the constant used in a header, GCC warns `-Winterference-size` even with no warning flags: "its value can vary between compiler versions or with different '-mtune' or '-mcpu' flags", and suggests defining your own constant if it is part of a public ABI.
  - clang 18 with libstdc++ (Linux container) does not define `__cpp_lib_hardware_interference_size` at all, so code that uses the constant does not compile there.
  - The OS report is not a safe source either: inside the Linux container on this Mac, `getconf LEVEL1_DCACHE_LINESIZE` prints 64, while `sysctl hw.cachelinesize` on the same host prints 128. A VM or container may report a different figure than the hardware's.
  - So: the value depends on compiler, library, target and what you ask. Put one named constant in your code, set it from the largest line size you have verified on the machines you deploy on (measured, not one query), and use `static_assert` to check your own struct offsets against that constant (card M5-PB9). A `static_assert` cannot read the OS or the CPU; a check against `sysconf`/`sysctl` would have to be a run-time check at start-up.
  - Holds for / re-check: values seen on Apple clang 21 (arm64 and x86-64 target), GCC 13.3 and clang 18.1.3 on aarch64 Linux. The draft calls the value "implementation-defined", "the minimum recommended offset between two concurrently-accessed objects" ([hardware.interference], C++23 draft N4950). It can change with any release or tuning flag; re-check when you change compiler, library, `-mcpu`/`-mtune`, or virtualization.
</details>

### Card M5-PB11 · Recall
Q: You pad each lock to `std::hardware_destructive_interference_size` so locks do not share a cache line. On this Mac, what does that constant say, what does the OS say the line size is, and what does a Linux container on the same Mac report? What does that teach you about trusting one source?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_consts.cpp -o "${TMPDIR:-/tmp}/line_consts" && "${TMPDIR:-/tmp}/line_consts" && sysctl hw.cachelinesize
   ```
   Answer format: the constant, the OS value, the container value, then how to check and one library difference.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 (libc++): destructive = 256, constructive = 64. GCC 13.3 (libstdc++, aarch64): also 256 and 64.
  - macOS `sysctl hw.cachelinesize`: 128. Linux container on the same Mac, `getconf LEVEL1_DCACHE_LINESIZE`: 64.
  - The constant is a compile-time choice by the library for a whole target family, not a measurement of your CPU; tools in different layers can disagree on the same hardware.
  - To check: print the constant, ask the OS, and if it matters, measure false sharing directly (two threads writing neighbouring fields at different distances).
  - Also: `sizeof(std::mutex)` is 64 with libc++ on macOS and 48 with libstdc++ on Linux aarch64, so a padding calculation done for one library does not hold for another.
  - Holds for / re-check: Apple clang 21.0.0 / libc++ on M1 Max macOS; GCC 13.3 / libstdc++ in a container on the same Mac. Library values can change with versions; re-check with `line_consts.cpp`.
</details>

### Card M5-PB12 · Spot the trap
Q: Before a "same cluster vs different cluster" experiment you must know the cache layout. On macOS query the keys below (with `sysctl`, or `sysctlbyname` from a program). In a Linux VM on the same machine read `/sys/devices/system/cpu/cpu0/cache/index*/shared_cpu_list` and `lscpu`. Do the two views agree?
   ```sh
   sysctl hw.perflevel0.physicalcpu hw.perflevel0.cpusperl2 hw.perflevel0.l2cachesize hw.perflevel1.physicalcpu hw.perflevel1.cpusperl2 hw.perflevel1.l2cachesize hw.cachelinesize
   ```
   Answer format: the macOS layout, the VM's report, then what you may and may not conclude.
<details><summary>Answer</summary>
Key:

  - macOS (M1 Max): 8 performance CPUs with 4 per L2 (two P-clusters, 12 MiB L2 each), 2 efficiency CPUs sharing a 4 MiB L2, 128-byte cache line.
  - The Linux VM (Docker Desktop, kernel `7.0.12-linuxkit`) reports all 10 CPUs sharing every cache level it exposes (`shared_cpu_list` 0-9 for index0-2), one cluster of 10 cores, and no cache `size` files.
  - These are guest reports: a guest-visible sharing mask is not proof about which physical cores share which cache. So these VM reports alone cannot be used to set up or validate "same cluster vs different cluster" placement. Read the topology on the host you measure, and say in results whether it came from bare metal or a VM.
  - Holds for / re-check: M1 Max host (macOS 27) and the Docker Linux VM on it. Values differ on every chip; the VM view depends on the hypervisor and its configuration. Re-run the probe on each machine.
</details>

### Card M5-PB13 · Apply
Q: Spec sheets give cache sizes, but you want to see the boundaries in your own code. `chase_sizes.cpp` builds a random cycle through a buffer, one entry per 128 bytes, and follows it with dependent loads for working sets from 16 KiB to 256 MiB (a pointer chase: each load's address comes from the previous load, so loads cannot overlap). First read your core's L1d and L2 sizes (`sysctl hw.perflevel0.l1dcachesize hw.perflevel0.l2cachesize` on Apple Silicon). Predict between which two sizes the first big step in ns per load appears. Then run three times:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/chase_sizes.cpp -o "${TMPDIR:-/tmp}/chase_sizes" && for i in 1 2 3; do "${TMPDIR:-/tmp}/chase_sizes"; done
   ```
   Answer format: the reported sizes, where the first step is, where the second rise starts, then what you use instead of the spec sheet.
<details><summary>Answer</summary>
Key:

  - The OS reports L1d = 131072 bytes (128 KiB) and L2 = 12582912 bytes (12 MiB) for the performance cores.
  - From 16 to 128 KiB the time per load is flat. The first big step is between 128 KiB and 256 KiB in every run, matching the reported L1d.
  - Time per load at 2 MiB is flat compared with 256 KiB, but at 8 MiB it is clearly higher than at 2 MiB in every run, and 8 MiB is below the reported 12 MiB L2. So the second rise starts before the spec-sheet L2 size, and the time keeps rising to 16 MiB and beyond.
  - Why it starts early is not isolated by this test. Candidates: random 128-byte-strided loads touch many 16 KiB pages, so address translation (TLB) misses join the cache misses; and other processes share the L2 cluster. Treat the knee you measure, not the listed capacity, as the usable size.
  - Use the same sweep on a new machine instead of copying latency tables, and read the knee positions, not the exact ns values (one machine, varies with load).
  - Holds for / re-check: direction and step location from one machine (M1 Max, P-cores; the run was not pinned, and the efficiency cores report a 64 KiB L1d, `hw.perflevel1.l1dcachesize`). This Mac uses 16 KiB pages (`pagesize` prints 16384) and the walk is random, so do not read the large-size values as pure cache latency. The TLB and sharing explanations above are candidates, not tested here. Re-check on each new chip.
</details>

### Card M5-PB14 · Apply
Q: A monitoring loop scans 6,000 sensor readings and uses only the `temp` field. Each `Reading` also holds a 24-byte device id, a 64-byte site name and 16 doubles of calibration data. `telemetry.cpp` counts (from the addresses, with no timing) how many distinct cache lines the scan touches if every reading is one struct, versus if `temp` and `seq` live in a small `ReadingHot` struct. Predict both counts for 64-byte and 128-byte lines, then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/telemetry.cpp -o "${TMPDIR:-/tmp}/telemetry" && "${TMPDIR:-/tmp}/telemetry"
   ```
   Also: why will the compiler not do this split for you?
   Answer format: the two sizes, the counts per line size, then why you split by hand.
<details><summary>Answer</summary>
Key:

  - `sizeof(Reading)` = 224, `sizeof(ReadingHot)` = 8.
  - Together: 6000 lines for both line sizes. Each record is longer than a line, so every read of `temp` needs its own line.
  - Split: 750 lines at 64 bytes and 375 at 128 bytes (Apple clang, vector start 128-aligned); GCC 13.3 on Linux gave 751 and 376, because that vector's start address was not line-aligned. Formula: about n x sizeof(hot) / line size.
  - So the split cuts the lines the scan pulls in by 8x (64-byte lines) or 16x (128-byte lines).
  - The compiler will not do it: `std::vector<Reading>` must hold its elements contiguously, and an array of `Reading` is `n` consecutive complete `Reading` objects ([dcl.array] p6, [vector.overview] p2, C++23 draft N4950), so `temp` of neighbouring elements is always `sizeof(Reading)` apart. Reordering members inside `Reading` cannot change that. Splitting into separate arrays changes the type and all the code that uses it, so you do it by hand.
  - Holds for / re-check: the counts follow from sizes and alignment, so they hold on any machine with the same struct sizes; they change if you change field types or packing. Whether fewer lines means a faster scan on a given chip must still be measured.
</details>

### Card M5-PB15 · Spot the trap
Q: A team writes `struct alignas(128) Slot { std::atomic<long> n{0}; };` and creates one with `new Slot`. The project still builds with `-std=c++14`. `overaligned_new.cpp` has that `new` plus a `static_assert` guarded by `__STDCPP_DEFAULT_NEW_ALIGNMENT__`. Without running anything, compile it (`-c`) with GCC 13.3 and with clang at `-std=c++14` and `-std=c++17`, always with `-Wall -Wextra`:
   ```sh
   clang++ -std=c++14 -Wall -Wextra -c modules/05-memory-perf/lab/practice/overaligned_new.cpp -o "${TMPDIR:-/tmp}/oa.o"
   g++ -std=c++14 -Wall -Wextra -c modules/05-memory-perf/lab/practice/overaligned_new.cpp -o "${TMPDIR:-/tmp}/oa.o"
   ```
   Which compiler says something in which mode, what do you need to add to get clang to say it, and what is the language rule behind it?
   Answer format: one line per compiler and mode, the rule, then the remedy.
<details><summary>Answer</summary>
Key:

  - GCC 13.3, `-std=c++14 -Wall -Wextra` (no extra flag): `warning: 'new' of type 'Slot' with extended alignment 128 [-Waligned-new=]`, with the note "uses 'void* operator new(std::size_t)', which does not have an alignment parameter" and "use '-faligned-new' to enable C++17 over-aligned new support".
  - clang (Apple clang 21 and clang 18.1.3), `-std=c++14 -Wall -Wextra`: silent. Adding `-Wover-aligned` gives `type 'Slot' requires 128 bytes of alignment and the default allocator only guarantees 16 bytes [-Wover-aligned]`. So "no warning" with the default warning set does not mean the allocation is fine.
  - `-std=c++17`, all three: no diagnostic with `-Wall -Wextra`, and the `static_assert` (`alignof(Slot) > __STDCPP_DEFAULT_NEW_ALIGNMENT__`) compiles, confirming `Slot` is over-aligned yet allocated correctly. Valid C++17 code (`new Slot`, `std::vector<Slot>`) run under `-fsanitize=alignment` with GCC and clang printed `new%128=0 vec%128=0`.
  - Rule: from C++17, a `new` of a type with new-extended alignment passes the alignment to the allocation function as a `std::align_val_t` argument ([expr.new] p14, C++17 draft N4659); before that plain `operator new` only promised the default alignment. In C++14 the construction at an address that does not satisfy `alignof` is not a valid program, so do not "test" it by running it: the optimizer may assume alignment holds, and a printed remainder proves nothing.
  - Remedy: build as C++17 or later, or `-faligned-new` on GCC, or in C++14 allocate with an alignment-aware allocator (C `aligned_alloc` (`<stdlib.h>`) / `posix_memalign` plus placement new, freed with the matching call). Treat the warning as an error (`-Werror=aligned-new=global` on GCC, `-Werror=over-aligned` on clang) in such projects.
  - Holds for / re-check: GCC 13.3 (libstdc++), clang 18.1.3 and Apple clang 21 (libc++), arm64 only; the warnings are compiler-specific and may change between versions. The `std::vector<Slot>` case is not covered by these warnings; only the direct `new` was checked. Nothing here says whether specific counters share a cache line.
</details>

### Card M5-PB16 · Spot the trap
Q: Given `#include <cstdint>` and `struct __attribute__((packed)) Pk { uint8_t tag; uint32_t qty; uint16_t venue; };` (`packed_ref.cpp`; `packed` is a GCC/Clang extension that removes padding, so members can sit at unaligned addresses), what do these two lines do on Clang and on GCC: `uint32_t* addr(Pk& p) { return &p.qty; }` and `void bump(uint32_t& q); void use(Pk& p) { bump(p.qty); }`? Why are both risky?
   ```sh
   clang++ -std=c++20 -Wall -Wextra -c modules/05-memory-perf/lab/practice/packed_ref.cpp -o "${TMPDIR:-/tmp}/packed_ref.o"
   g++ -std=c++20 -Wall -Wextra -c modules/05-memory-perf/lab/practice/packed_ref.cpp -o "${TMPDIR:-/tmp}/packed_ref.o"
   ```
   Answer format: one line for the address, one for the reference, then the risk and the fix.
<details><summary>Answer</summary>
Key:

  - Taking the address: both compilers warn `-Waddress-of-packed-member` ("may result in an unaligned pointer value").
  - Binding a reference: GCC 13 rejects it (`error: cannot bind packed field 'p.Pk::qty' to 'uint32_t&'`). Apple clang 21 accepts it with no diagnostic under `-Wall -Wextra`.
  - Risk: a `uint32_t*` or `uint32_t&` promises 4-byte alignment. Code through it may use instructions that require alignment (or the optimizer may assume it).
  - **Fix:** copy the field into a local, work on the local, and write it back.
  - Holds for / re-check: Apple clang 21.0.0, GCC 13.3 and clang 18.1.3 (Linux aarch64), `-std=c++20 -Wall -Wextra`; clang 18.1.3 also accepts the binding silently. Diagnostics can change between versions; re-check by compiling `packed_ref.cpp`.
</details>

### Card M5-PB17 · Apply
Q: `clamp.cpp` sets negative values to zero in four ways: element type `int` or `signed char`, and the store written as `if (a[i] < 0) a[i] = 0;` or as `a[i] = a[i] < 0 ? 0 : a[i];`. Before you run anything, predict for AArch64: on plain NEON (the default target), which of the four loops end up with whole-vector stores? Then predict what changes when the target also has SVE (predicated vector instructions). Run the script and read the count lines. Why can the `if` form not simply use a plain vector store?
   ```sh
   bash modules/05-memory-perf/lab/practice/clamp_sve.sh clang++
   clang++ -std=c++20 -O2 -g -fsanitize=thread modules/05-memory-perf/lab/practice/clamp_reader.cpp modules/05-memory-perf/lab/practice/clamp.cpp -o "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" always
   ```
   (The second command runs one clamping thread against one reader thread, first with the `if` form, then with the always-store form.)
   Answer format: why the `if` form is special, the NEON counts, the SVE counts, then what the always-store rewrite requires.
<details><summary>Answer</summary>
Key:

  - The `if` form writes only some elements. The compiler may not turn that into "write every element back": a store the program never asked for could conflict with another thread's read of an element the original code only reads ([intro.races] Note 22, C++23 draft N4950). So the `if` form needs a store that writes only the selected elements, or it stays one test and one store per element.
  - Plain NEON, Apple clang 21: `clamp32_if` and `clamp8_if` have 0 vector stores (20 and 48 conditional branches). Both `always` forms have 1 plain vector store.
  - With `-march=armv8-a+sve`, Apple clang 21: `clamp32_if` has 2 predicated stores (`st1w`) and `clamp8_if` has 3 (`st1b`), and no plain vector store. clang 18.1.3 gives the same 2 and 3. GCC 13.3 with SVE has 1 predicated store in each of the four loops.
  - So the same source goes from per-element stores to predicated vector stores when the target gains a predicated store. The source did not change; the target did.
  - Writing the store unconditionally (the `always` form) adds stores. They require that no unsynchronized access in another thread conflicts with them; concurrent readers can make this rewrite invalid. (Under ThreadSanitizer with one reader thread over 1,024 positive ints: the `if` form printed a checksum and no report; the `always` form reported a data race, a read of size 8 against a previous write by the clamping thread.) A concurrent non-atomic writer already conflicts with the original loop's reads.
  - Holds for / re-check: the NEON `if` loops contain vector work but scalar stores; do not call the whole loop scalar. Counts are from cross-compiled listings, `-O2` for clang and `-O2`/`-O3` for GCC. No SVE code was run (the M1 Max has none). Counts of the `always` forms under SVE differ between compilers and are not part of the answer. In these listings NEON has no masked store; the architecture manuals were not opened. GCC 13.3 on aarch64 without SVE (`-O3`) leaves both `if` forms scalar with `not vectorized: control flow in loop`. A newer compiler or a different element type can change every line; re-check by running the script on your compiler and target.
</details>

### Card M5-PB18 · Spot the trap
Q: `calib_stop()` adds an offset to sensor readings and returns at the first fault marker. `calib_all()` converts every reading, ORs a "fault seen" flag, and finds the fault position after the loop. Both return the same index. Assume the input and output ranges are disjoint, all n input elements are readable, and every input-plus-offset value is representable in `int32_t`. The input has 1000 readings with a fault at index 400, and both output buffers start filled with -1. Predict: in how many output elements do the two functions differ? Is the second a drop-in replacement?
   ```sh
   bash modules/05-memory-perf/lab/practice/calib_report.sh clang++
   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/calib.cpp modules/05-memory-perf/lab/practice/calib_alias.cpp -o "${TMPDIR:-/tmp}/calib_alias" && "${TMPDIR:-/tmp}/calib_alias"
   ```
   (The second command calls both functions in place, `in == out`.)
   Answer format: the count, why a test can miss it, when the rewrite is valid, then what you get in exchange.
<details><summary>Answer</summary>
Key:

  - They differ in 600 elements: indices 400 to 999. The convert-all version writes past the fault. The stopping version leaves those elements untouched.
  - With no fault the buffers are identical (0 differences). So a test that uses only clean input, or compares only return values, passes and hides the change.
  - The rewrite is valid only if the contract allows it: the caller must not rely on the tail being untouched, and nobody else may write those elements meanwhile. It also needs the input unchanged until its second search, so it is not an in-place replacement: called with `in == out` on input `{0, INT32_MIN, 2}` and offset 7, the stopping version returns 1, but the convert-all version overwrites the marker, its unbounded search runs off the array, and ASan reports a stack-buffer-overflow READ of size 4 in `calib_all`. Aliasing is another correct answer to the drop-in question.
  - What you get in exchange: all three compilers checked report the stopping loop as not vectorized and the convert-all loop as vectorized.
  - Habit: before comparing two variants for speed, compare their full outputs on inputs that trigger the rare case.
  - Holds for / re-check: element counts hold on any conforming compiler (they follow from the source). The vectorization reports are from Apple clang 21 (`-O2`), clang 18.1.3 (`-O2`) and GCC 13.3 (`-O3`, aarch64). The reason text differs by version: clang 18 says `could not determine number of loop iterations`, Apple clang 21 says `Cannot vectorize early exit loop with writes to memory`, GCC 13 says `control flow in loop`. The 600 count assumes the disjoint buffers stated in the question. A later compiler may vectorize the stopping loop; re-run the script to see.
</details>

### Card M5-PB19 · Spot the trap
Q: A colleague replaces a branchy filter with a branch-free one. The job: keep the odd order ids.
   ```cpp
   std::size_t keep_odd_nobranch(const std::uint32_t* in, std::size_t n, std::uint32_t* out) {
       std::size_t m = 0;
       for (std::size_t i = 0; i < n; ++i) {
           out[m] = in[i];
           m += in[i] & 1u;
       }
       return m;
   }
   ```
   The caller counts the odd ids first and allocates `out` with exactly that many slots. The unit test passes. What is wrong, why can a test miss it, and how do you catch it? Build `slack.cpp` and run all four cases (`b` = branchy, `n` = no branch, `k` = input ordered so the kept ids come last):
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/slack.cpp -o "${TMPDIR:-/tmp}/slack" && "${TMPDIR:-/tmp}/slack" b && "${TMPDIR:-/tmp}/slack" b k && "${TMPDIR:-/tmp}/slack" n k && "${TMPDIR:-/tmp}/slack" n
   ```
   Answer format: the bug, what the detector says, why a test passes, then the fix.
<details><summary>Answer</summary>
Key:

  - The branch-free form writes `out[m]` for every input element, kept or not. A rejected element is written to the next free slot and overwritten later, or never.
  - If a rejected element arrives after the output is already full, that write lands one slot past the end. With 4 kept ids and a 4-slot buffer, input `11 13 15 17 20 22` writes `out[4]`.
  - AddressSanitizer (ASan, a compiler mode that instruments the memory accesses that survive optimization) reports it: "heap-buffer-overflow ... WRITE of size 4 ... located 0 bytes after 16-byte region", in `keep_odd_nobranch`. Seen with Apple clang 21 and with GCC 13.3 on Linux.
  - The bug depends on the data. The same ids in the order `20 22 11 13 15 17` (last element kept) run clean under ASan, and the branchy version is clean for both orders. So a test with a lucky input passes.
  - Without ASan, on the builds tested (Apple clang 21, GCC 13.3, clang 18 at `-O1`), the bad run prints "kept 4 of 6" and exits 0, so nothing visible fails. It is undefined behaviour, so other outcomes on other builds are possible.
  - **Fix:** a branch-free pack needs spare room in the output. For this scalar form one extra element is enough (checked at `-O1` under ASan with Apple clang 21, 2047 cases: all 0/1 inputs of length 0 to 10 with capacity kept+1 were clean and counted correctly, with the function marked `[[gnu::noinline]]` so its stores could not be optimized away; the control with capacity == kept failed with a heap-buffer-overflow WRITE of size 4). The invariant behind it: at every store m <= the total kept, and a buffer of `n` elements is always safe. A version that stores a whole vector at a time needs more spare room; that is not tested here. Put the requirement in the function's contract and run the tests under ASan with an input that ends in rejected elements.
  - Holds for / re-check: a property of the algorithm, not of a compiler, so it does not expire. The ASan wording is from Apple clang 21.0.0 (arm64) and GCC 13.3.0 (aarch64 Linux). Re-check by running the four commands.
</details>

### Card M5-PB20 · Apply
Q: `fsum.cpp` adds up a `float` array three ways: `std::accumulate(p, p+n, 0.0f)`, `std::reduce(p, p+n, 0.0f)` and `std::reduce(std::execution::unseq, p, p+n, 0.0f)`. No `-ffast-math`. For each one predict: will the compiled code contain a vector add (on arm64, an `fadd` on `v` registers such as `fadd v0.4s, ...`)? Write your prediction down, then check on your own compiler and library (Apple clang needs `-fexperimental-library` for the third; without it the file does not compile). `fadd_kinds.sh` prints the count of vector and scalar `fadd` per function:
   ```sh
   clang++ -std=c++20 -O3 -fexperimental-library -S modules/05-memory-perf/lab/practice/fsum.cpp -o "${TMPDIR:-/tmp}/fsum.s" && sh modules/05-memory-perf/lab/practice/fadd_kinds.sh "${TMPDIR:-/tmp}/fsum.s"
   g++ -std=c++20 -O3 -S modules/05-memory-perf/lab/practice/fsum.cpp -o "${TMPDIR:-/tmp}/fsum.s" && sh modules/05-memory-perf/lab/practice/fadd_kinds.sh "${TMPDIR:-/tmp}/fsum.s"
   ```
   (On Linux, repeat with `-fopenmp-simd` added, and the same with `clang++`.) Does your answer depend on the compiler, the library and the flags?
   Answer format: yes/no for the three functions per toolchain, then why.
<details><summary>Answer</summary>
Key:

  - Observed (vector `fadd` present? accumulate / reduce / reduce(unseq)): GCC 13.3 + libstdc++, `-O3`: no / no / no. With `-fopenmp-simd` or `-fopenmp`: no / no / YES.
  - clang 18.1.3 + libstdc++ 13, `-O3`: no / YES / no. With `-fopenmp-simd` or `-fopenmp`: no / YES / YES.
  - Apple clang 21 + libc++ with `-fexperimental-library`, `-O3`: no / no / YES. Without the flag the file does not compile ("no member named 'unseq' in namespace 'std::execution'"). `-fopenmp-simd` changed nothing here.
  - A prediction is right if it matches the listing from the toolchain you actually used. "Plain `reduce` stays scalar" is right on GCC and Apple clang and wrong on clang 18. "`reduce(unseq)` needs an OpenMP flag to vectorize" is right on both libstdc++ toolchains tested and not what Apple clang showed. No single rule covers all three.
  - Why: `std::accumulate` must add strictly left to right ([accumulate] p2), so no compiler may regroup float additions into lanes (scalar everywhere tested). `std::reduce` may group in any order ([reduce] p7, `GENERALIZED_SUM`), but that is only permission. libstdc++ hand-groups four elements in plain `reduce`, which clang 18 turned into vector adds and GCC did not. For `unseq`, libstdc++ marks its loop with an OpenMP `simd` pragma, and the pragma only takes effect with an OpenMP flag.
  - Habit: check the generated code for your exact compiler and library. Do not carry the answer from one toolchain to another.
  - Quick probe: the feature-test macro `__cpp_lib_execution`. GCC 13.3 prints 201902. Apple clang 21 does not define it, with or without `-fexperimental-library`.
  - Holds for / re-check: three toolchains, all arm64/aarch64: Apple clang 21.0.0 with libc++ (`_LIBCPP_VERSION` 220106); GCC 13.3.0 and clang 18.1.3, both with libstdc++ 13 (Ubuntu 24.04 container). Order rules from C++20 draft N4861. Not checked on x86, where the instruction has a different name (a packed add such as `addps` or `vaddps`), nor clang 18 with libc++. Results change with compiler, library and flags: re-check after every toolchain upgrade.
</details>

### Card M5-PB21 · Recall
Q: (a) In one sentence each: what does passing `std::execution::unseq` permit the library to do, and what does it oblige the library to do? (b) A `std::for_each(std::execution::unseq, ...)` call runs a lambda that locks a `std::mutex`, updates a shared total, and unlocks. Is that allowed? Look it up in [algorithms.parallel.exec].
   Answer format: (a) two sentences, (b) yes or no with the reason, then the rule.
<details><summary>Answer</summary>
Key:

  - (a) It permits the library to run the element operations out of order and interleaved with each other on the calling thread. It obliges nothing about speed: the draft says an implementation may fall back to sequential execution.
  - (b) No. It is undefined behaviour. With `unseq`, user code must not call a "vectorization-unsafe" standard library function. A function is vectorization-unsafe if it is specified to synchronize with another call ([algorithms.parallel.defns] p4), and `mutex::lock` is specified that way ("Prior unlock() operations on the same object synchronize with this operation", [thread.mutex.requirements.mutex] p11). The draft's note gives the practical reason: interleaving on one thread plus a blocking lock risks deadlock.
  - Rule to keep: a policy moves duties to the caller (no locks under `unseq` and `par_unseq`; no data races under `par` and `par_unseq`) and gives freedoms to the library. It never promises a faster loop. To know what you got, read the generated code (card M5-PB20).
  - Holds for / re-check: C++20 text; `unseq` does not exist before C++20 (`unsequenced_policy` has 0 hits in the C++17 draft N4659). The wording is from the N4861 draft, not the published ISO standard. Re-check against the draft for your language version by searching for `algorithms.parallel.exec`.
</details>

### Card M5-PB22 · Apply
Q: A team builds with `-ftrivial-auto-var-init=zero` (the compiler writes zeros into every automatic variable that has no initializer). A 1 KiB local histogram filled by a call into another file is in a hot function, so they opt that one variable out. Developer A writes `uint32_t hist[256] [[indeterminate]];`, developer B writes `uint32_t hist[256] __attribute__((uninitialized));`. Both compile. Which one actually removes the fill on Apple clang 21 and on GCC 13, and how do you check without timing anything?
   ```sh
   clang++ -std=c++2c -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -
   g++ -std=c++23 -O2 -DTRY_STD -ftrivial-auto-var-init=zero -S modules/05-memory-perf/lab/practice/fill_optout.cpp -o -
   ```
   Answer format: the check, one line per compiler, then the lesson.
<details><summary>Answer</summary>
Key:

  - Check: compile with `-O2 -S` and count calls to `memset`/`bzero` in each function. Careful: GCC's identical-code folding (ICF) merges `std_opted_out` into a tail jump to `plain`, so a per-function count shows 0 for the wrong reason. Run it twice, with and without `-fno-ipa-icf`, or look for the `b _Z5plainv` jump.
  - Apple clang 21 (`-std=c++2c`): `[[indeterminate]]` gives `warning: unknown attribute 'indeterminate' ignored`; the fill call is still there (1 call). `__attribute__((uninitialized))`: 0 calls.
  - GCC 13.3 (`-std=c++23`): `warning: 'indeterminate' attribute directive ignored`. Default build: `std_opted_out` counts 0 `memset` calls, but only because it was folded into a jump to `plain` (1 call). With `-fno-ipa-icf`: `std_opted_out` = 1 call. `__attribute__((uninitialized))`: 0 calls.
  - Lesson: an unknown standard attribute is ignored with only a warning. Turn on `-Werror=unknown-attributes` / `-Werror=attributes`, and confirm in the asm.
  - Holds for / re-check: Apple clang 21.0.0 arm64 and GCC 13.3 aarch64, `-O2` (GCC 13.3 does not know `-std=c++2c`; use `-std=c++23`). Newer compilers that implement C++26 will accept `[[indeterminate]]`; re-check with the same count after any compiler upgrade.
</details>

### Card M5-PB23 · Apply
Q: `llvm-mca` is LLVM's static pipeline model: it reads assembly and predicts cycles per loop iteration. The files `accK.s` (K = 1, 2, 3, 6, 10) sum doubles with K independent `fadd` chains (K loads at fixed offsets, K `fadd`s, one pointer bump). Predict cycles per element for each K, then run the model and divide "Total Cycles" by 1000*K. For K=10, add `-resource-pressure` and name the resource that limits it.
   ```sh
   for k in 1 2 3 6 10; do llvm-mca -mtriple=aarch64 -mcpu=apple-m1 -iterations=1000 modules/05-memory-perf/lab/practice/mca/acc$k.s | grep "Total Cycles"; done
   llvm-mca -mtriple=aarch64 -mcpu=apple-m1 -iterations=1000 -resource-pressure modules/05-memory-perf/lab/practice/mca/acc10.s
   ```
   Answer format: cycles per element for each K, the limiting resource at K=10, then the method.
<details><summary>Answer</summary>
Key:

  - The model gives `fadd` latency 5 (Instruction Info). Cycles per element: K=1 about 5.0, K=2 about 2.5, K=3 about 1.67, K=6 about 0.83, K=10 about 0.50.
  - Up to K=6 the cost is latency / K: the loop waits on the add chain.
  - At K=10 the limit is the load units: the two `CyUnitLS` resources show 5.00 per iteration each (10 loads on 2 units), while the three vector units show 3.33.
  - Method: add chains until cost per element stops falling as latency / K; then read the resource-pressure view to see which unit limits you next (here the loads, before the three add units are full).
  - Holds for / re-check: model output from LLVM 22.1.6 (Homebrew `llvm-mca`; another LLVM version may print other figures), `apple-m1` model. A model name is not a hardware claim: real hardware can differ, so time it on the target before trusting a ratio.
</details>

### Card M5-PB24 · Apply
Q: `escape_opaque.cpp` declares `void observe(const int*);` with no body (an *opaque* call: the compiler cannot see what it does) and has four functions that sum an `int` array into a local `acc`: `sum_local` (no call), `sum_escape_each` (calls `observe(&acc)` inside the loop), `sum_escape_before` (calls `observe(&acc)` once, before the loop), `sum_snapshot` (copies `acc` into `int snap` and calls `observe(&snap)` inside the loop). Predict, for each, whether `acc` is loaded from and stored to the stack on every loop step. Then check the loop bodies (and with GCC, `g++` on Linux, the same way):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/escape_opaque.cpp -o -
   ```
   Answer format: yes or no for each of the four functions, then the rule.
<details><summary>Answer</summary>
Key:

  - `sum_escape_each`: yes. Each step stores `acc` to its stack slot before the call and reads it back after (Apple clang `str`/`ldr [sp,#12]`; GCC `str`/`ldr [sp,4]`; x86 clang 19.1 does `addl %eax, 4(%rsp)`). The callee might have kept the pointer and changed `acc` (the `const` in the parameter does not stop that, because `acc` itself is not const).
  - `sum_snapshot`: no. `acc` stays in a register; only `snap` is stored, and never read back. Escaping a copy protects the original.
  - `sum_escape_before`: no. One reload after the single call, then the loop runs on registers (Apple clang and GCC -O3 even vectorize it). With no opaque call inside the loop, nothing can observe `acc` there.
  - `sum_local`: no memory traffic for `acc` at all.
  - Rule to keep: what blocks optimization is a call to unseen code while the variable's address is out, not the variable's type or the syntax around it.
  - Holds for / re-check: Apple clang 21 arm64, GCC 13.3 aarch64 (-O2 and -O3), gcc 14.3 and clang 19.1 x86-64 (Compiler Explorer) at -O2. Whether the loop also vectorizes depends on compiler and -O level (GCC 13.3 -O2 did not vectorize `sum_escape_before`; -O3 did). Re-check by reading the loop body after any compiler upgrade.
</details>

### Card M5-PB25 · Spot the trap
Q: To fill a large output buffer without polluting the cache, a teammate rewrites `dst[i] = v` as `__builtin_nontemporal_store(v, &dst[i])` and says "now every store is non-temporal, on any platform." (A *non-temporal* or streaming store carries a hint not to keep the line in cache; on x86 it uses the weakly ordered *write-combining*, WC, protocol.) `stream_store.cpp` has both versions (runtime length `n`). Build it for arm64, for x86-64 (Compiler Explorer, clang), and with GCC (`g++` on Linux). What do you find, and what extra step does x86 need before another thread may read the buffer? Then build `stream_publish.cpp` (x86-64 clang on Compiler Explorer, `-std=c++20 -O2`): which of its three publication patterns emits the hardware fence?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/stream_store.cpp -o - | grep -E 'stnp|str'
   g++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/stream_store.cpp -o -
   ```
   Answer format: GCC result, x86-64 count, arm64 result per compiler, then the fence finding and the habit.
<details><summary>Answer</summary>
Key:

  - GCC (13.3 here, gcc 15.3 on Compiler Explorer) rejects it: `'__builtin_nontemporal_store' was not declared`. It is a clang builtin, not portable C++.
  - x86-64 clang 19.1: every store in `fill_stream` is `movnti` (9 of 9, including the scalar tail).
  - arm64 differs by compiler. Apple clang 21: four `stnp` sites in the main loop (16 ints per step), then ordinary `str q0` (vector) and `str w1` (scalar) tail loops. Linux clang 18: two `stnp` sites (8 ints per step) and only a scalar `str w1` tail. So a fill shorter than the main-loop step gets zero streaming stores. Clang's documentation says the builtin is a hint and does not guarantee a non-temporal instruction.
  - x86 ordering: non-temporal stores use the WC protocol, which is weakly ordered, so the Intel manual says to use `SFENCE` or `MFENCE` with `MOVNTI` when other processors may read the data. In `stream_publish.cpp`, a C++ release fence followed by a flag store, and a release store of the flag, both compile (clang 19.1 x86-64) to `movnti; mov; ret` with no fence. Only the version with an explicit `_mm_sfence()` emits `sfence`. So: do the fill, `_mm_sfence()` (or the target's equivalent), then publish with a release store, and have the reader use an acquire load. The builtin and a C++ release operation alone did not add the fence here.
  - Habit: after using any "hint" builtin, grep the listing for the instruction you expected and count how many stores actually carry it.
  - Holds for / re-check: clang 18, 19 and 21; GCC up to 15.3 lacks the builtin. The arm64 lowering follows from STNP being a pair instruction and differs by compiler version; re-check with `-S` and a grep for `movnt`/`stnp`. The fence result is one clang version on x86-64 (Compiler Explorer, compile only); it is about emitted instructions, not a proof about the C++ memory model. Whether the Intel ordering text applies to your memory type depends on the platform manual.
</details>

### Card M5-PB26 · Apply
Q: You are writing a read-only hardware report for a tool, and one line must say "cache line size". Gather six answers on one Apple M1 Max: (1) `sysctl hw.cachelinesize` on macOS; (2) `getconf LEVEL1_DCACHE_LINESIZE` in a Linux VM on the same Mac; (3) `interference_macro.cpp`, which prints `__cpp_lib_hardware_interference_size` and both `std::hardware_*_interference_size` constants, built with Apple clang; (4) the same with GCC 13.3 (aarch64 Linux); (5) the same with clang 18.1 and libstdc++; (6) the same with x86-64 gcc 13.3 on Compiler Explorer. Classify each answer as an OS or guest report, a compile-time recommendation, or unavailable. What can the report honestly say?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/interference_macro.cpp -o "${TMPDIR:-/tmp}/interference_macro" && "${TMPDIR:-/tmp}/interference_macro"
   ```
   Answer format: the six values with their class, then what the report should print.
<details><summary>Answer</summary>
Key:

  - (1) 128, an OS report on the host. (2) 64, a report by the guest Linux. Same chip, different reporters, different numbers.
  - (3) and (4): destructive 256, constructive 64: compile-time recommendations for that target, fixed when the program is built. (6) x86-64 gcc 13.3 gives 64 / 64. Same compiler family, different target, different constants.
  - (5): clang 18.1 with libstdc++ does not define `__cpp_lib_hardware_interference_size` (aarch64 Linux and x86-64 both): unavailable, and code using the constant does not compile there. Test the feature macro.
  - The standard calls the destructive value an implementation-defined "minimum recommended offset" ([hardware.interference] p1); it is not a measurement of any cache.
  - Lesson: a reported line size (what the OS or guest says the cache uses) and a recommended interference spacing (what the implementation suggests for avoiding sharing) are different quantities that need not be equal. A report should print each with its source and say which are unavailable. Reading one does not invalidate the other.
  - Limit: a compile-time constant describes the compiled target and says nothing about the CPU the program later runs on; the probe above does not decide which number a program should use.
  - Holds for / re-check: one M1 Max (macOS host and a Linux VM on it), Apple clang 21, GCC 13.3, clang 18.1.3, and x86-64 gcc 13.3 / clang 18.1 on Compiler Explorer, re-run 2026-11-18. Values differ per CPU, OS, VM and library version; a newer clang may define the constant (not checked here). Re-check: run the probes on each target.
</details>

### Card M5-PB27 · Apply
Q: Two classes each contain three `uint64_t` words and define class-specific allocation and deallocation functions. `SizedOnly` defines only `operator delete(void*, std::size_t)`. `Both` defines that overload and `operator delete(void*)`. Predict which overload `delete new T` calls for each class. Run the probe: does adding a sized overload mean it is selected?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/delete_selection.cpp -o "${TMPDIR:-/tmp}/delete_selection" && "${TMPDIR:-/tmp}/delete_selection"
   ```
   Answer format: the overload called for each class, then the rule and the habit.
<details><summary>Answer</summary>
Key:

  - Both object sizes are 24 here. `SizedOnly` prints `only: sized bytes=24`; `Both` prints `both: unsized`.
  - Adding the unsized class-specific overload changes the selection: in class scope, the one without a `std::size_t` parameter is chosen. Merely providing a sized overload does not show that the allocator receives the size.
  - Instrument the selected deallocation function before reasoning about saved allocator work. This test measures selection and bytes, not allocator speed.
  - Holds for / re-check: Apple clang 21, C++20, ordinary scalar new/delete on these classes. Arrays, over-alignment, inheritance and destroying delete are outside this probe; re-check the selection before extending it to those cases.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
