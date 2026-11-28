# Lesson 07 — Practice Bank, part 2 of 9 (optional): Layout and Alignment

**Time: about 24 minutes (estimate)** for 4 cards (8 + 6 + 4 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: array of structs against struct of arrays, over-aligned `new`, packed fields, and sized deallocation.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

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

Next: part 3, `lesson-07-practice-bank-03-false-sharing.md` (about 29 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
