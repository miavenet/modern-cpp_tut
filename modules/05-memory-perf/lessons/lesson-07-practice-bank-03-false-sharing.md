# Lesson 07 — Practice Bank, part 3 of 9 (optional): False Sharing and Interference Size

**Time: about 29 minutes (estimate)** for 4 cards (5 + 8 + 8 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: measure false sharing, guard a layout against it, and check what the constant really promises.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

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
  - Holds for / re-check: `offsetof`, `static_assert` and `std::is_standard_layout_v` are standard C++; `offsetof` needs a standard-layout class ([support.types.layout] p1). Diagnostic wording differs between compilers (Apple clang 21 also prints the evaluated expression). The guard checks offsets against your constant `kLine`; it cannot tell you the hardware line size (card M5-PB7). Checked on Apple clang 21, and on GCC 13.3 / clang 18 on aarch64 Linux.
</details>

### Card M5-PB10 · Apply
Q: A shared header used by two builds (an arm64 and an x86-64 build of the same library, or the same code built with different `-mcpu`/`-mtune`) contains `struct Padded { alignas(std::hardware_destructive_interference_size) long v; };` (`hdis_hdr.h`). Card M5-PB7 gave you the constant's values on this Mac. Here the question is the interface: what can differ between the two builds, and what does GCC say about it? Check the x86-64 value from the Mac, then build `use_hdr.cpp` with GCC and `-Wall` (on Linux), and try `hdis.cpp` with clang and libstdc++ if you have it.
   ```sh
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos -S modules/05-memory-perf/lab/practice/hdis.cpp -o -
   g++ -std=c++20 -Wall -c modules/05-memory-perf/lab/practice/use_hdr.cpp -o "${TMPDIR:-/tmp}/use_hdr.o"
   ```
   Answer format: what differs between the builds, what GCC says, what clang with libstdc++ does, then what you put in the header instead.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 gives 256 for arm64 but 64 for the x86-64 target (the listing loads `$64` for both constants), so `sizeof(Padded)` and `alignof(Padded)` are 256 in one build and 64 in the other: the same header gives two incompatible layouts. Code compiled one way cannot share that struct with code compiled the other way.
  - GCC 13.3 on aarch64 warns `-Winterference-size` even with no warning flags: "its value can vary between compiler versions or with different '-mtune' or '-mcpu' flags", and "if this use is part of a public ABI, change it to instead use a constant variable you define".
  - clang 18 with libstdc++ (Linux container) does not define `__cpp_lib_hardware_interference_size` at all, so the header does not compile there.
  - So the constant belongs only in code built once for one target, never in a type shared across builds. In a header, use one named constant of your own (for example `inline constexpr std::size_t kLine = 128;`), set from the largest line size you have verified on the machines you deploy on, and `static_assert` your struct offsets against it (card M5-PB9). A `static_assert` cannot read the OS or the CPU; a check against `sysconf`/`sysctl` would have to be a run-time check at start-up.
  - Holds for / re-check: Apple clang 21 (arm64 and x86-64 target, compile only for x86-64), GCC 13.3 and clang 18.1.3 on aarch64 Linux. The draft calls the value "implementation-defined" ([hardware.interference], C++23 draft N4950). It can change with any release or tuning flag; re-check when you change compiler, library, `-mcpu`/`-mtune` or target.
</details>

---

## Close

Next: part 4, `lesson-07-practice-bank-04-cache-line.md` (about 27 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
