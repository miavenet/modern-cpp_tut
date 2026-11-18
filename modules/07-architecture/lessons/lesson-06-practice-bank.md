# Lesson 06 — Practice Bank (optional): Fair Benchmarks and What the Build Really Did

**Time: about 89 minutes** for 9 cards (12 + 10 + 3 + 5 + 12 + 15 + 5 + 15 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a hash table over 30x slower at lookup because its hash has only 787 distinct values; the bucket counts show it before any timer does.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++); counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB1 · Apply
Q: How much machine code does one more distinct instantiation of a template cost in the program you ship? `tsize.cpp` instantiates `work<I>` (fill, `std::sort`, `std::lower_bound` and erase on a `std::vector<Rec<I>>`) for NTYPES distinct record types of the same layout. For NTYPES = 1, 2, 4, 8, 16: compile to an object file, then link it with `tmain.cpp`, once with the default link and once with `-Wl,-no_deduplicate`. Read section sizes with `size -m`. Before running, predict: does the per-type slope of `__text` in the object file match the slope in the linked binary? One loop does all 15 builds (each size prints the object, the default binary, then the no-dedup binary):
   ```sh
   P=modules/07-architecture/lab/practice
   for n in 1 2 4 8 16; do clang++ -std=c++20 -O2 -DNDEBUG -DNTYPES=$n -c $P/tsize.cpp -o "${TMPDIR:-/tmp}/t$n.o" && clang++ -std=c++20 -O2 $P/tmain.cpp "${TMPDIR:-/tmp}/t$n.o" -o "${TMPDIR:-/tmp}/b$n" && clang++ -std=c++20 -O2 $P/tmain.cpp "${TMPDIR:-/tmp}/t$n.o" -Wl,-no_deduplicate -o "${TMPDIR:-/tmp}/b${n}nd" && echo "NTYPES=$n" && size -m "${TMPDIR:-/tmp}/t$n.o" "${TMPDIR:-/tmp}/b$n" "${TMPDIR:-/tmp}/b${n}nd"; done
   ```
   Answer format: bytes per extra type for the object file, the default link and the no-dedup link, then one line on what to measure.
<details><summary>Answer</summary>
Key:

  - Object files: `__text` grows linearly, about 5.5 KiB per extra type (5752 B at 1 type, 89956 B at 16: about 5614 B/type).
  - Linked binary, default link: about 0.5 KiB per type (5772 to 13176 B: about 494 B/type). The linker's deduplication pass merged most of the per-type copies. With `-Wl,-no_deduplicate` (an Apple linker flag; `size -m` is the Mach-O `size`, so these commands are macOS-only) the binary is back at about 5614 B/type.
  - So measure the final linked binary, with the link flags you ship; object-file sizes can overstate the cost many times over. The slope, not one size, is the per-type cost.
  - Also list the non-code sections: `__gcc_except_tab` grows 24 B/type (40 to 400 B) in both. `__compact_unwind` (352 to 3744 B in the objects, in the `__LD` segment) is linker input: the binary has no such section and carries `__unwind_info` instead (176 to 360 B by default, 176 to 720 B without dedup).
  - The record types here share one layout, which favours merging. Types whose code differs were not measured, so measure your own program; do not assume this ratio.
  - Holds for / re-check: Apple clang 21, libc++, ld64-954.16 (Mach-O), `-O2 -DNDEBUG`, arm64, this one template. These are byte counts, not timings. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), ELF with GNU ld (bfd; linker version not recorded): the default link did not merge (about 2.3 KB (2306-2378 B) per type in object and binary), and `-fuse-ld=gold -Wl,--icf=all` gave about 22 B per type. ELF objects keep the instantiations in COMDAT `.text.*` sections, so sum those. Re-check: link the real program both ways and compare `size` output.
</details>

### Card M7-PB2 · Apply
Q: Build a correctness check before any timing. Three functions answer "how many values in this sorted array are strictly less than x?": a reference that uses a prefix-count table, a fast one that uses `std::lower_bound`, and one that uses `std::upper_bound`. All three get the same 100,000 seeded queries and fold their answers into a checksum. Predict which ones match the reference, then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/rank_check.cpp -o "${TMPDIR:-/tmp}/rank_check" && "${TMPDIR:-/tmp}/rank_check"
   ```
   What do you do with a contestant whose checksum differs, even if it is the fastest?
   Answer format: which pass and why, then what you do with a failing contestant.
<details><summary>Answer</summary>
Key:

  - The `lower_bound` version matches (PASS); the `upper_bound` version does not (FAIL). `upper_bound` returns the first element greater than x, so it also counts the elements equal to x; with values drawn from 0..999 nearly every query value is present.
  - A contestant that fails did different work. Its time is meaningless; discard the run and fix it first.
  - Make the reference a different, simple algorithm, so it is unlikely to share the contestant's bug. Fix the seed so all contestants get the same input. Run the check every time you change the size, the code or the machine you test on.
  - Holds for / re-check: the method holds anywhere. The checksum values depend on the seed and the PRNG; `std::mt19937_64` is fully specified by the standard and the answers are counts, so they should match on any conforming library.
</details>

### Card M7-PB3 · Recall
Q: Before timing several implementations of the same operation against each other, what check must every run pass, and why is a mixed workload (several operations folded into one number) hard to interpret? (Short on time? This card recalls the lesson of card M7-PB2; `:skip` it if you just did that card.)
   Answer format: the check in one line, then why a mixed number misleads, with an example.
<details><summary>Answer</summary>
Key:

  - A correctness check: every contestant answers the same seeded queries and produces the same checksum as a simple reference implementation; a mismatch voids the run.
  - A mixed number can hide opposite effects. Example (arithmetic): implementation X takes 10 ns per insert and 30 ns per erase, Y 20 and 20. On a 50/50 mix both average 20 ns, a tie, yet X is 2x faster at inserts and 1.5x slower at erases. Report each operation in its own column.
  - Holds for / re-check: a method; it holds for any language or machine. The correctness check is the one in card M7-PB2.
</details>

### Card M7-PB4 · Apply
Q: You read about a new C++26 container and want to try it on your Mac today. Write the two-line check that tells you (1) which standard library you are compiling against, and (2) whether that library ships the C++26 hive container. What does it print here? Compare with:
   ```sh
   clang++ -std=c++2c -O2 modules/07-architecture/lab/practice/hive_check.cpp -o "${TMPDIR:-/tmp}/hive_check" && "${TMPDIR:-/tmp}/hive_check"
   ```
   Answer format: the two macros to test, then what this library prints.
<details><summary>Answer</summary>
Key:

  - Check the library macro: `_LIBCPP_VERSION` (libc++) or `__GLIBCXX__` (libstdc++). Here: libc++ 220106.
  - Check the feature-test macro from `<version>`: `__cpp_lib_hive`. The C++26 working draft lists it (value 202502L); the C++23 draft has no hive at all.
  - Here, with -std=c++2c, `__cpp_lib_hive` is not defined and `__has_include(<hive>)` is 0, so this library does not ship it. You would need a third-party implementation, and then your result is about that implementation, not "std".
  - Holds for / re-check: Apple clang 21 / libc++ 220106. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ (`__GLIBCXX__` 20240904) has no `<hive>` either; GCC 13 rejects `-std=c++2c`, and with `-std=c++23` it printed the same. Expires when the library ships `<hive>`. Re-check by recompiling the same program after each toolchain update.
</details>

### Card M7-PB5 · Apply
Q: You compare two order-book node allocators. Allocator A grabs one large block at startup and touches it during setup. Allocator B asks the OS for memory as the book grows. A wins clearly. Before believing that the allocation algorithm is better, list three differences that are not the algorithm, and one measurement for each that would expose it. Then run this program to see the second one:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/cache_warm.cpp -o "${TMPDIR:-/tmp}/cache_warm" && "${TMPDIR:-/tmp}/cache_warm"
   ```
   Answer format: three differences, each with its measurement.
<details><summary>Answer</summary>
Key:

  - First-touch page faults land inside B's timed region but not A's. Count faults per timed run (`getrusage` `ru_minflt`); compare B with its memory pre-touched.
  - A's memory is cache-warm: setup just walked it, so its first timed accesses hit cache. Fix by evicting (stream a large unrelated buffer) or by timing both from the same cache state. In the program both buffers are pre-faulted (0 faults), yet in 15 reps over 3 process runs on a quiet machine every random pointer chase over 1 MiB was slower after streaming 256 MiB through than right after setup. The microseconds rise under load (one machine), so judge by the pattern, not the figures.
  - Different work: check both produce the same book state (a checksum) before comparing times.
  - Accept other non-algorithm differences if each comes with a measurement that exposes it.
  - Holds for / re-check: the method holds on any OS. The cache numbers are from one machine and depend on cache sizes. The test uses a dependent random chase so that hardware prefetching cannot hide misses.
</details>

### Card M7-PB6 · Apply
Q: A cache of recent events is a `std::unordered_map` keyed by nanosecond timestamps. Someone wrote a custom hash, `ts / 1'000'000` ("hash at millisecond granularity"). The program builds 2^19 keys spaced about 1.5 us apart and loads them into two maps: one with `std::hash<uint64_t>`, one with the custom hash. For each map it first prints the bucket counts (`bucket_count()`, how many buckets are non-empty, and the largest `bucket_size()`), then times insert and a shuffled lookup of every key, 7 reps. Before running, predict the non-empty count and the largest bucket for the custom hash from the key spacing alone.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/hashq.cpp -o "${TMPDIR:-/tmp}/hashq" && "${TMPDIR:-/tmp}/hashq"
   ```
   Answer format: your two predicted numbers with the arithmetic, then what the timings showed.
<details><summary>Answer</summary>
Key:

  - Prediction from arithmetic: 2^19 keys x 1.5 us cover about 786 ms, so the custom hash has only about 787 distinct values; each millisecond holds about 1,000,000 / 1500 = 667 keys.
  - Measured (identical in every run): custom hash 787 non-empty buckets out of 823,117, largest bucket 667. `std::hash`: 388,181 non-empty, largest 7. The counts show the bug without any timing.
  - Times agree with the counts. In every run here the custom hash's lookup median was over 30x the `std::hash` one, and its insert median over 4x. The milliseconds themselves changed from run to run with load, so they are not given here.
  - Technique: inspect the bucket-size distribution (`bucket_count`, `bucket_size`) first. It is cheap, deterministic and machine-independent, and it explains the timing.
  - Holds for / re-check: the counts depend only on the keys, the hash and the library's bucket-count policy (libc++ here chose 823,117 buckets); another library will choose other bucket counts, but the custom hash still gives about 787 distinct values. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ chose 712,697 buckets; the custom hash still gave 787 non-empty, largest 667. The timings are this machine's. Re-check by printing the same counts on the target.
</details>

### Card M7-PB7 · Spot the trap
Q: A write-up compares two order-book layouts from one run each and reports "layout B is 1.5x slower than A". First see how steady a timing is: run the hash-table program from card M7-PB6 alone, then two copies at once, two or three rounds of each, and compare the min/median/max columns and the bucket counts:
   ```sh
   "${TMPDIR:-/tmp}/hashq"; "${TMPDIR:-/tmp}/hashq" & "${TMPDIR:-/tmp}/hashq" & wait
   ```
   (Build it first with the card M7-PB6 command if needed.) Why is the write-up not evidence yet, and what is the minimum you would ask for?
   Answer format: why in one or two lines, then the list of what you would ask for.
<details><summary>Answer</summary>
Key:

  - Same binary, same keys, same machine, 7 reps per cell. The bucket counts and the checksum were identical in every run, alone or paired; the times were not. With a second copy running, the timings changed with no change to the code, and by how much differed from round to round: in some rounds the `std::hash` medians barely moved, in others they rose a lot. So no fixed size of the effect is given here; your rounds will show their own.
  - So machine state alone moves the timings, by an amount you cannot know from one run, and a single run per side cannot show it. Without several runs and their spread you cannot tell a real 1.5x from noise; the deterministic counts are what stayed fixed.
  - Ask for: several runs per configuration, a median plus a spread (min/max or IQR), fixed inputs (seeded engine), the machine state (pinning, other load), and a deterministic counter that does not depend on timing, such as instructions retired or allocations made.
  - Holds for / re-check: the spread sizes are this desktop Mac's, for this load; spreads vary from run to run. Re-check by printing the spread every time.
</details>

### Card M7-PB8 · Apply
Q: `devirt/shape.h` and `devirt/shape.cpp` hold an interface `Shape` with one virtual function `area` and its only implementation `Square final`. `devirt/use.cpp` has two noinline functions with the same loop: `sum_iface(Shape&, ...)` and `sum_leaf(Square&, ...)`. Build four ways and look for `blr` (indirect branch) vs `bl` (direct call) in `objdump -d`: (A) no LTO, one object file; (B) add `-flto` and link both files; (C) `-flto -fwhole-program-vtables -fvisibility=hidden`; (D) as (A) but with `final` removed (edit a copy of the three files). Predict each before you look.
   ```sh
   D=modules/07-architecture/lab/practice/devirt
   clang++ -std=c++20 -O2 -c $D/use.cpp -o "${TMPDIR:-/tmp}/use.o" && objdump -d -r "${TMPDIR:-/tmp}/use.o"
   clang++ -std=c++20 -O2 -flto $D/use.cpp $D/shape.cpp -o "${TMPDIR:-/tmp}/B" && objdump -d "${TMPDIR:-/tmp}/B"
   clang++ -std=c++20 -O2 -flto -fwhole-program-vtables -fvisibility=hidden $D/use.cpp $D/shape.cpp -o "${TMPDIR:-/tmp}/C" && objdump -d "${TMPDIR:-/tmp}/C"
   cp -R $D "${TMPDIR:-/tmp}/devirtD" && sed -i '' 's/Square final/Square/' "${TMPDIR:-/tmp}/devirtD/shape.h" && clang++ -std=c++20 -O2 -c "${TMPDIR:-/tmp}/devirtD/use.cpp" -o "${TMPDIR:-/tmp}/useD.o" && objdump -d -r "${TMPDIR:-/tmp}/useD.o"
   ```
   Hint: `objdump -d` may label the first function in an object with a local label such as `ltmp0` instead of its name; `objdump -t` gives each symbol's address, so you can match them.
   Answer format: one line per build (A to D), then the lesson in one line.
<details><summary>Answer</summary>
Key:

  - (A) No LTO: `sum_iface` uses `blr`. `sum_leaf` uses a direct `bl` to `Square::area`: the static type is `Square`, and `final` means no class can override it, so this one file is enough.
  - (D) Remove `final` and `sum_leaf` goes back to `blr`: a subclass in another file could override `area`.
  - (B) LTO alone: `sum_iface` still has four `blr` (the loop is unrolled). Inside the noinline function, `s` is a parameter, so its dynamic type is not known there. With this clang, LTO alone did not use the fact that `Square` is the only implementation.
  - (C) Whole-program vtables with hidden visibility: no `blr` remains; the two functions became identical and were folded, so `main` calls `sum_iface` twice. Clang's docs say these whole-program vtable features "use whole-program information" and apply only to classes with hidden LTO visibility, which `-fvisibility=hidden` gives. It works from the class hierarchy, not from knowing the object.
  - Lesson: whether a virtual call stays indirect depends on exact flags and visibility. Check the instructions, not the slogan.
  - Holds for / re-check: the `sed -i ''` above is BSD/macOS `sed`; GNU `sed` takes `sed -i` with no empty argument. Apple clang 21; GCC has its own devirtualization passes and flags. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 with `-O2 -flto` devirtualized `sum_iface` speculatively: it compares the vtable slot with `Square::area`, runs the inlined body on a match and `blr` otherwise (`-fopt-info` reports it). `-fwhole-program-vtables` is only safe if no other module (for example a plugin loaded at run time) adds implementations of the class. Re-check by rebuilding and grepping for `blr` (arm64) or `call *` (x86) on your compiler.
</details>

### Card M7-PB9 · Apply
Q: You benchmark a filter that keeps the index of every price above a limit: `if (px[i] > limit) out[m++] = i;`. To save time you generate a short block of prices and repeat it to fill 16M elements. The branchy loop looks almost free. What could make that result too optimistic for live data, and what one change to the input tests it? Build it and run both inputs (arguments: target pass rate, repeat period; 0 = never repeats):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/filter.cpp -o "${TMPDIR:-/tmp}/filter" && "${TMPDIR:-/tmp}/filter" 0.8 1000 && "${TMPDIR:-/tmp}/filter" 0.8 0
   ```
   Answer format: the cause in one line, the input change in one line, then what you measured.
<details><summary>Answer</summary>
Key:

  - A branch predictor can learn a repeating sequence of outcomes. Then a repeated block mispredicts far less than fresh data with about the same pass rate, so the benchmark hides the misprediction cost.
  - Test: keep the pass rate, remove the repetition (period 0), and compare. Also print the realized pass rate of each input, since "the same rate" is only approximate: here 0.794 (period 1000) vs 0.800 (no repeat).
  - First confirm the loop really has a branch: in the `-O2` listing (`-S`), `branchy` has a `cmp` + `b.ls` per element. A branch-free control (`out[m] = i; m += px[i] > limit;`) should not care about the input.
  - Measured here, in every run: branchy with period 1000 was over 3x faster per element than with no repetition, and the branch-free control ran at the same speed on both inputs. The ns/elem figures moved a little with load, so they are not given here.
  - The predictor here also learned a 10,000-element period (at least as fast as period 1000 in every run). A 100,000-element period landed between the two and changed from run to run: partly learned. So "long enough to be safe" must be measured too, not guessed.
  - Rule: generate benchmark inputs without repetition, or replay recorded production data.
  - Holds for / re-check: how long a pattern the predictor learns depends on the core, so the periods above are this M1 Max's. Another compiler version or flag set may turn the branch into branch-free code; check the listing each time. Re-check on each new CPU by sweeping the period (for example 1000, 10^4, 10^5, 0). On Linux, `perf stat -e branch-misses` shows the mechanism directly.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
