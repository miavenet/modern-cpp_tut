# Lesson 06 — Practice Bank (optional): Fair Benchmarks and What the Build Really Did

**Time: about 220 minutes** for 26 cards (12 + 10 + 3 + 5 + 12 + 15 + 5 + 15 + 12 + 8 + 10 + 10 + 10 + 10 + 5 + 5 + 8 + 6 + 5 + 3 + 15 + 15 + 6 + 4 + 8 + 3 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a hash table over 30x slower at lookup because its hash has only 787 distinct values; the bucket counts show it before any timer does.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

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

### Card M7-PB10 · Apply
Q: `pack.cpp` has three loops over a byte buffer. `upper` writes an upper-cased copy: input byte `i` always goes to output byte `i`. `strip_spaces` copies every byte that is not a space: `if (in[i] != ' ') out[m++] = in[i];`. `strip_spaces_nobranch` does the same without an `if`: `out[m] = in[i]; m += (in[i] != ' ');`. Before you compile, write down which of the three the compiler will vectorize at `-O3`. Then ask the compiler instead of reading assembly (`pack_restrict.cpp` is the same with `__restrict` on both pointers):
   ```sh
   clang++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
   g++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack.cpp -o /dev/null -fopt-info-vec-optimized -fopt-info-vec-missed
   clang++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/pack_restrict.cpp -o /dev/null -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize
   ```
   Answer format: which loop is vectorized, the refusal for the other two, then what the obstacle is.
<details><summary>Answer</summary>
Key:

  - Only `upper` is vectorized. Apple clang 21 reports "vectorized loop (vectorization width: 16, interleaved count: 4)" for its loop. GCC 13.3 reports "loop vectorized using 16 byte vectors".
  - Both compaction loops (copying only the kept elements so they sit next to each other) are refused. Apple clang 21 and clang 18: "loop not vectorized: value that could not be identified as reduction is used outside the loop". GCC 13.3: "not vectorized: control flow in loop" for the `if` form and "data ref analysis failed" for the no-branch form.
  - Removing the `if` did not help. The obstacle is not the branch. It is that the output position `m` depends on every earlier element, so the compiler cannot say where element `i` will be written.
  - It is not an aliasing problem either: with `__restrict` on both pointers clang still gives the same "could not be identified as reduction" refusal for both loops. Only the extra "cannot identify array bounds" remark goes away.
  - A wider instruction set did not change it on these compilers: GCC 13.3 with `-march=armv8.2-a+sve` and clang 18 with `-march=armv9-a` still vectorize only `upper`.
  - Habit: the vectorizer report is a one-line command. Run it before you claim a loop "is SIMD" or "cannot be SIMD".
  - Holds for / re-check: Apple clang 21.0.0 (arm64) and GCC 13.3.0 and clang 18.1.3 (aarch64 Linux container), at `-O3`. Not checked on x86. A newer compiler may learn to vectorize this loop shape; the wording of the remarks also changes between versions. Re-check by running the commands above on your compiler and target flags.
</details>

### Card M7-PB11 · Apply
Q: `levels.cpp` has two loops: `add_fee` (can be vectorized) and `running_total` (not vectorized by the tested compilers: a loop-carried prefix-sum dependence, each element needs the one before). You ask for the vectorizer report at `-O0`, `-O1`, `-Os`, `-O2` and `-O3`. Predict for GCC and for clang: at which levels is the report empty, and does an empty report mean "nothing to complain about"?
   ```sh
   bash modules/07-architecture/lab/practice/levels_report.sh clang++
   bash modules/07-architecture/lab/practice/levels_report.sh g++
   ```
   Answer format: the empty levels per compiler, what `add_fee` gets at the other levels, then the method.
<details><summary>Answer</summary>
Key:

  - GCC 13.3: 0 report lines at `-O0`, `-O1` and `-Os`. Both loops are scalar there. The report is empty because the vectorizer did not run, not because the loops are fine.
  - GCC 13.3 at `-O2`: `add_fee` is `missed` ("Loop costings not worthwhile"). At `-O3`: `optimized: loop vectorized`. `running_total` is `missed` at both, with a dependence reason.
  - clang (Apple 21 and 18.1.3): 0 lines at `-O0`. At `-O1`, one bare `loop not vectorized` per loop with no reason. At `-Os`, reasons appear and `add_fee` is declined by the cost model. At `-O2` and `-O3`, `add_fee` is vectorized.
  - Method: keep a loop you know vectorizes in the same file as a control. If the control gets no line, the report channel is dead at this level and silence tells you nothing.
  - Ask for the report with the flags you ship. A report from another level describes other code.
  - Holds for / re-check: `running_total` being declined is a fact about these compilers, not a ban: unsigned addition is associative, and a hand-written SIMD scan (shifts plus a carry between chunks) matched the scalar loop on 1030 cases (a counterexample, not a claim any compiler does this). The line counts are for these three compilers on arm64/aarch64. Which passes run at each level is a compiler-version decision and has changed before, so re-run the script after an upgrade. Not checked on x86.
</details>

### Card M7-PB12 · Apply
Q: Your team wiki says: "A loop that can return in the middle never vectorizes." `search_fixed.cpp` has two search-only loops with no stores: one takes a pointer and a run-time length, the other a reference to a `std::array` of 1024 elements. Name at least two things that could make the wiki line wrong for a given build. Then test it with every compiler you have:
   ```sh
   bash modules/07-architecture/lab/practice/search_report.sh clang++
   bash modules/07-architecture/lab/practice/search_report.sh g++ O3
   ```
   Answer format: two or more things, what each compiler reported, then what the wiki line needs.
<details><summary>Answer</summary>
Key:

  - Examples; others are also correct (target ISA, optimization level or flags). Compiler version: Apple clang 21 reports `vectorized loop (vectorization width: 4, ...)` for the `std::array` version. clang 18.1.3 and GCC 13.3 (`-O3`) leave both versions scalar.
  - Loop shape, i.e. what the compiler knows about the data. In the same Apple clang 21 build the pointer version stays scalar: `Cannot vectorize potentially faulting early exit loop`. A vector read past the exit point could touch memory the scalar loop never reads; with a fixed-size array the compiler knows all of it is readable.
  - GCC's release notes for version 14 announce vectorization of loops with early breaks, at first only when the data size is known at compile time or the alignment can be shown (gcc.gnu.org/gcc-14/changes.html; GCC 14 was not run here).
  - So the wiki line needs a compiler version, a target, and a loop shape next to it, and a command that re-checks it.
  - Holds for / re-check: checked on Apple clang 21.0.0 (arm64), clang 18.1.3 and GCC 13.3.0 (aarch64 Linux), at the flags in the script. Not checked on x86 or on newer GCC. Record the exact compiler string (`--version` output) with a result. Re-check on every compiler upgrade.
</details>

### Card M7-PB13 · Spot the trap
Q: A teammate decides whether a loop was vectorized by counting listing lines that mention `xmm`, `ymm` or `zmm`. For `total()` in `scalar_fp.cpp` (a plain sum of `double`s, compiled for x86-64 with `-O2 -mavx2`) the count is above zero, so they report "vectorized". What is wrong with the test, and what would you count instead?
   ```sh
   bash modules/07-architecture/lab/practice/scalar_fp.sh clang++
   ```
   Answer format: why the test is wrong, what the counts show, then what to count instead.
<details><summary>Answer</summary>
Key:

  - On x86-64 by default all `double` arithmetic uses the xmm registers, scalar code included (SSE2 is the baseline; x87 code is possible, for example for `long double` or with `-mfpmath=387`). So "mentions xmm" does not mean SIMD work.
  - Measured: `total` has 12 lines naming xmm, 0 naming ymm, 9 scalar-double instructions (`vaddsd`) and 0 packed arithmetic ones. It is scalar. The compiler says so too: `loop not vectorized: cannot prove it is safe to reorder floating-point operations`.
  - The control `twice()` has packed arithmetic (5 packed-double arithmetic instructions on Apple clang 21, 4 on clang 18) and ymm registers.
  - Beware a name-pattern census: counting every mnemonic ending in `ps`/`pd` gives 3 hits on the scalar `total` (`vxorpd` x2, `vxorps`: register zeroing), so it would also report "vectorized". Count packed arithmetic by name (`vaddpd`, `vmulpd`, ... or integer `vp...` on ymm/zmm) and whole-vector stores. Always run the census on one loop you know is scalar and one you know is vectorized, to test the census itself.
  - Holds for / re-check: x86-64 assembly produced by cross-compiling with Apple clang 21 and clang 18.1.3 (same result on both); not run on GCC or on an x86 machine. The register-name trap is specific to x86; on AArch64 the names differ, but the rule "test your census on a known-scalar and a known-vector loop" still applies. The result for `total()` depends on the target: both clangs report it as vectorized for `--target=aarch64-linux-gnu`, so re-run for your target.
</details>

### Card M7-PB14 · Apply
Q: Compile `count_above(const int* a, int n, int t)` at `-O2 -std=c++20` with x86-64 GCC 13.3 and x86-64 clang 18.1 on Compiler Explorer, and locally with Apple clang (arm64) and GCC 13.3 (aarch64). Which listings contain SIMD compare instructions? Then try GCC with `-O3`.
   ```sh
   clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -
   g++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -
   ```
   Answer format: one line per compiler at `-O2`, one for GCC `-O3`, then the lesson.
<details><summary>Answer</summary>
Key:

  - At `-O2`, clang vectorizes: x86-64 clang 18.1 uses `xmm` registers (`pcmpgtd`); Apple clang arm64 uses `cmgt` vector compares.
  - At `-O2`, GCC 13.3 does not: one element per iteration (x86: `cmpl`/`setl`/`addl`, no branch on the element). Same on aarch64.
  - With `-O3`, GCC vectorizes too (the x86 listing has `xmm`; aarch64 has `cmgt`/`.4s`).
  - Lesson: `-O2` is a per-compiler bundle of passes: clang's includes the loop vectorizer; GCC 13's uses the very-cheap cost model and left this loop scalar. Read the listing, not the flag name.
  - Holds for / re-check: GCC 13.3 and clang 18.1 (x86-64, Compiler Explorer), Apple clang 21 and GCC 13.3 (aarch64, local). GCC's `-O2` vectorizer rules changed in GCC 12 (gcc.gnu.org/gcc-12/changes.html: vectorization at `-O2` with the very-cheap cost model) and may change again; re-check per release.
</details>

### Card M7-PB15 · Recall
Q: You want each compiler to tell you, in words, whether it unrolled a loop (copied the body so fewer branches run), without reading assembly. Which flag do you pass to clang and which to GCC, and what does each print for a 12-iteration loop it unrolls completely?
   ```sh
   clang++ -std=c++20 -O2 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -Rpass=loop-unroll
   g++ -std=c++20 -O3 -c modules/07-architecture/lab/practice/hashloop.cpp -o /dev/null -fopt-info-loop
   ```
   Answer format: one flag and message per compiler, then how far to trust remarks.
<details><summary>Answer</summary>
Key:

  - clang: `-Rpass=loop-unroll` prints `remark: completely unrolled loop with 12 iterations [-Rpass=loop-unroll]`. Add `-Rpass-missed=loop-unroll` to see refusals.
  - GCC: `-fopt-info-loop` prints `optimized: loop with 11 iterations completely unrolled`. GCC counts latch runs, so it says 11 for a 12-iteration loop: read the number with care.
  - Remarks are a fast first filter; confirm with the listing, because other passes (vectorizer, later cleanups) can still change the shape.
  - Holds for / re-check: Apple clang 21 (Mac), GCC 13.3 (Linux aarch64). Remark wording and counting differ between releases. Re-check by running the flags on a loop whose trip count you know.
</details>

### Card M7-PB16 · Apply
Q: A teammate tested a 12-step hash loop (`h = h * 31u + p[i]` for `i` from 0 to 11) with GCC at `-O2`, saw a real loop, and wrote in the wiki: "GCC keeps short fixed loops rolled; only clang unrolls them." What setting change would break this rule, and how do you show it in two commands? (A backward branch, one whose target is earlier in the listing, means a loop in this small function.)
   ```sh
   g++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/hashloop.cpp -o -
   g++ -std=c++20 -O3 -S modules/07-architecture/lab/practice/hashloop.cpp -o -
   ```
   Answer format: the setting, the two counts, then what a rule about unrolling must name.
<details><summary>Answer</summary>
Key:

  - The `-O` level. GCC 13.3 at `-O2` kept the loop (1 backward branch); at `-O3` it unrolled completely (0 backward branches). Apple clang unrolled completely at `-O2` and kept the loop at `-O1`.
  - Show it: build the same file with `-O2 -S` and `-O3 -S`, and count backward branches in each listing (or use `-fopt-info-loop`). Do not count a fixed list of conditional-branch mnemonics: they differ per ISA and per compiler.
  - So a rule about unrolling must name compiler, version, `-O` level and target, and be re-run when any of them changes.
  - Holds for / re-check: GCC 13.3 aarch64 and Apple clang 21 arm64, one source file. Re-check on your toolchain; release notes for loop passes are the place to look when the shape changes.
</details>

### Card M7-PB17 · Spot the trap
Q: You compare your inlined data structure against a rival library that you call through a small accessor compiled in its own file (its own translation unit). `boundary/lib.cpp` holds `extern "C" uint64_t book_spread(const Book*)`; `boundary/app.cpp` calls it 1000 times in a loop. Build twice, without and with `-flto` (link-time optimization) on all three lines, and count calls to the accessor in `main`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/07-architecture/lab/practice/boundary/lib.cpp -o "${TMPDIR:-/tmp}/lib.o"
   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/07-architecture/lab/practice/boundary/app.cpp -o "${TMPDIR:-/tmp}/app.o"
   clang++ -O2 "${TMPDIR:-/tmp}/lib.o" "${TMPDIR:-/tmp}/app.o" -o "${TMPDIR:-/tmp}/app_bin" && "${TMPDIR:-/tmp}/app_bin"
   objdump -d --no-show-raw-insn "${TMPDIR:-/tmp}/app_bin" | awk '/<_?main>:/{p=1;next} /^$/{p=0} p' | grep -cE '(bl|call).*_?book_spread'
   ```
   What does the count tell you about the fairness of the comparison?
   Answer format: the count without and with LTO, then the fix.
<details><summary>Answer</summary>
Key:

  - Without LTO: 1 call instruction to `book_spread` in `main` (Apple clang 21, and GCC 13.3 and clang 18 on Linux all gave 1), executed every iteration. The compiler cannot see the body across the translation-unit boundary, so it cannot inline it or optimize around it.
  - With `-flto`: 0 calls (Apple clang 21 and GCC 13.3; the clang 18 Linux LTO build also gave 0). The link-time optimizer inlined it. Every build prints the same result (1000).
  - So the rival paid for a real function call in every iteration and your structure did not. That is a property of the harness, not of the rival.
  - **Fix:** build every contender the same way (same language, same translation unit or same LTO setting). Check by counting calls in the listing, as above.
  - Holds for / re-check: Apple clang 21 (Mach-O, symbols have a leading underscore: `_main`, `_book_spread`) and GCC 13.3 / clang 18 on aarch64 Linux (ELF, no underscore), which is why the pattern uses `_?`. `bl` is the arm64 call instruction; on x86 the call is `call`, which the pattern also matches (not run on x86 here). If the awk range finds no `main`, the count is 0 for the wrong reason: print the range first. LTO behaviour varies with compiler and linker; re-check with the count command after any toolchain change.
</details>

### Card M7-PB18 · Spot the trap
Q: You want a background "noise" function that keeps the memory system busy, so you can measure another routine under load. `noise.cpp` has two versions: `noise_plain(n)` allocates a `std::vector<std::uint32_t>` of n elements and fills it with a hash of the index, never reading it back; `noise_volatile(n)` does the same fill through a `volatile std::uint32_t*`. Without running anything, compile to assembly and, for each function, count the calls to `operator new` (`_Znwm`) and say whether it contains a store loop:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/07-architecture/lab/practice/noise.cpp -o -
   ```
   Does the noise function you wrote actually run?
   Answer format: one line per function per compiler, then the habit.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 (arm64) and clang 18 (Linux aarch64): `noise_plain` contains 0 calls to `operator new` and no store loop; what is left is a length check that jumps to the `throw` path (6 instructions on Apple clang). The compiler removed the allocation and the fill (dead-store elimination: it deletes writes whose value can never be observed). `noise_volatile` keeps 1 call to `operator new` and the store loop.
  - GCC 13.3 (aarch64): both versions keep the call to `operator new` and the store loop (`str w2, [x3], 4` in the loop). So the same source is a real noise generator with one compiler and an empty function with another.
  - Habit: before you trust a stress or noise generator, look at its listing (or time it alone and check that the number fits the work). A `volatile` access is observable behaviour, so the loop must stay.
  - Holds for / re-check: counts hold for the three compilers named, at `-O2`. Other versions or flags may keep or delete these stores; that is why the listing is checked every time. `volatile` guarantees the accesses stay, not that they hit DRAM or evict anything useful.
</details>

### Card M7-PB19 · Recall
Q: Two request handlers get the same fixed 2 ms of extra work (say a new auth check). Handler A takes 4 ms before and 6 ms after. Handler B takes 20 ms before and 22 ms after. A report says "A slowed by 50%, B only 10%, so B copes better with the change." What is wrong with that conclusion, and what should the report show? Then: for what kind of change would a percentage be the right summary?
   Answer format: what is wrong, what to report, then when a percentage fits and how to tell.
<details><summary>Answer</summary>
Key:

  - Both handlers lose exactly 2 ms per request. The percentage divides one fixed cost by each handler's own baseline, so the shorter handler always looks "worse" for the same absolute loss.
  - After the change A (6 ms) is still faster than B (22 ms). The change did not make B the better handler.
  - Report the absolute time before and after and the absolute difference. A percentage is the right summary when the extra cost scales with the work, for example a build that makes every instruction 10% slower; then A would lose 0.4 ms and B 2 ms. To tell the two cases apart, measure two input sizes: a constant difference means a fixed cost, a constant ratio means a scaling cost.
  - Holds for / re-check: plain arithmetic (2/4 = 50%, 2/20 = 10%, 6 < 22; 10% of 4 = 0.4 and 10% of 20 = 2), so it holds for any machine. The numbers are made up for the exercise.
</details>

### Card M7-PB20 · Apply
Q: Before timing any concurrent copy scheme, build a self-checking stress test that can expose inconsistent snapshots (torn reads: a copy that mixes parts of two different writes). In `torn_detect.cpp` the writer always stores the same value into all eight words (each word a `std::atomic<long>`, written relaxed). The reader copies the eight words and counts snapshots whose words differ. Mode `none` copies with no protocol; mode `seq` wraps the copy in a version-counter check. Predict the torn count for each mode (zero or non-zero), then run three times:
   ```sh
   clang++ -std=c++20 -O2 modules/07-architecture/lab/practice/torn_detect.cpp -o "${TMPDIR:-/tmp}/torn_detect" && for r in 1 2 3; do "${TMPDIR:-/tmp}/torn_detect" none && "${TMPDIR:-/tmp}/torn_detect" seq; done
   ```
   Answer format: zero or non-zero per mode, what the invariant costs, then what a passing `seq` run proves.
<details><summary>Answer</summary>
Key:

  - `none`: non-zero torn count, and `seq`: torn = 0, in all 3 of 3 runs here. Neither the exact counts nor "most" snapshots is guaranteed on another run or machine; scheduling can give an unprotected run with no observed tear.
  - The "all words equal" invariant needs no separate reference copy, but it is not free: the check costs several compares and branches per snapshot.
  - A deliberately broken control (`none`) shows the checker can see tearing in the cases run. A passing run of `seq` supplements a memory-model argument for the protocol; it does not prove the protocol correct. Each word is atomic, so the unprotected mode is torn but not undefined behaviour.
  - Holds for / re-check: direction (non-zero vs zero) on M1 Max with Apple clang 21, 3 runs. If `none` ever shows 0, make the copy longer or the writer faster before trusting the test. Re-check on each target.
</details>

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

### Card M7-PB23 · Recall
Q: You want to test the claim "no interrupts reached CPU 0 during my run" by diffing `/proc/interrupts` before and after, inside a Docker container. The diff is empty. What else must you check before you accept the claim? Run these in a default container:
   ```sh
   ls -l /proc/interrupts /dev/null; wc -l < /proc/interrupts; grep interrupts /proc/self/mountinfo
   ```
   Answer format: what the three commands show, then the rule.
<details><summary>Answer</summary>
Key:

  - In this container `/proc/interrupts` is a character device 1,3, the same as `/dev/null`, has 0 lines, and `mountinfo` shows `/null` mounted over it. The runtime masks the file. An empty diff means "no data", not "no interrupts".
  - `/proc/stat` is still readable there and its `intr` line has totals, but not the per-CPU split you need.
  - Rule: before trusting a counter, check that it exists and moves (read it twice around something that must change it). Measure isolation questions on the host, not in a container.
  - Holds for / re-check: Docker with default flags (kernel 7.0.12-linuxkit). Other runtimes or privileged containers may expose the real file. The masking was observed, not looked up in Docker's documentation.
</details>

### Card M7-PB24 · Apply
Q: `seq_tsan.cpp` has one writer and one reader using a version counter around a 4-word struct. With `-DCOPY=0` both sides copy the words with plain loads and stores; with `-DCOPY=1` every word goes through `std::atomic_ref<long>` relaxed loads/stores. A teammate builds `COPY=0` with Apple clang ThreadSanitizer (TSan), sees no report in three runs, and says "TSan says it is race-free". Build both variants with TSan on every toolchain you have and run each three times:
   ```sh
   for c in 0 1; do clang++ -std=c++20 -O1 -g -fsanitize=thread -Wall -Wextra -DCOPY=$c modules/07-architecture/lab/practice/seq_tsan.cpp -o "${TMPDIR:-/tmp}/seq_tsan$c" && for r in 1 2 3; do "${TMPDIR:-/tmp}/seq_tsan$c"; done; done
   ```
   (Repeat with `g++` and `clang++` on Linux.) What do you see, and what do you conclude?
   Answer format: one line per toolchain, the GCC compile-time warning, then the conclusion.
<details><summary>Answer</summary>
Key:

  - Apple clang 21 (macOS arm64): no report for either variant in 3 of 3 runs, although the reader retried hundreds of thousands of times, so the threads did overlap.
  - GCC 13.3 and clang 18.1.3 (Linux aarch64): `COPY=0` gives "WARNING: ThreadSanitizer: data race" in 3 of 3 runs each; `COPY=1` gives none.
  - GCC also prints at compile time: "'atomic_thread_fence' is not supported with '-fsanitize=thread' [-Wtsan]". The detector does not model fences, so its verdict on fence-based code is not reliable either way.
  - Conclusion: a quiet sanitizer is not proof. The plain copy is a data race by the standard ([intro.races] p21, C++23 draft N4950) no matter what one tool says. Run more than one toolchain, and prefer atomic (relaxed) word copies, which are race-free by construction.
  - Holds for / re-check: results for the three toolchains named, on arm64. Sanitizer runtimes change between releases (fence support may be added); re-check by running both variants on every new toolchain, and keep the `COPY=1` variant as a "should be quiet" control.
</details>

### Card M7-PB25 · Spot the trap
Q: Which of these is undefined behaviour in C++20, and which is not? (A) `static_cast<std::int32_t>(3.0e9f)`. (B) `static_cast<std::int64_t>(UINT64_MAX)`. How would you catch (A) in a test run? `float_cast.cpp` does (A) when given the argument `big`:
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=float-cast-overflow modules/07-architecture/lab/practice/float_cast.cpp -o "${TMPDIR:-/tmp}/float_cast" && "${TMPDIR:-/tmp}/float_cast" big
   g++ -std=c++20 -O1 -g -fsanitize=undefined modules/07-architecture/lab/practice/float_cast.cpp -o "${TMPDIR:-/tmp}/float_cast" && "${TMPDIR:-/tmp}/float_cast" big
   ```
   Answer format: (A) and (B) with the rule each, then the sanitizer setting per compiler.
<details><summary>Answer</summary>
Key:

  - (A) is undefined: a floating value whose truncated value does not fit the integer type ([conv.fpint] p1). UBSan with `-fsanitize=float-cast-overflow` reports "3e+09 is outside the range of representable values of type 'int'"; the program then printed 2147483647, which you must not rely on.
  - (B) is defined since C++20: integer-to-integer conversion gives the value congruent modulo 2^N, here -1 ([conv.integral] p3). Before C++20 it was implementation-defined, still not undefined.
  - Rule: float-to-int is the dangerous direction. Range-check before the cast, and keep `-fsanitize=float-cast-overflow` in your test builds. It is part of `-fsanitize=undefined` on Clang, but not on GCC 13.3: there `-fsanitize=undefined` printed nothing for (A) and you must add `-fsanitize=float-cast-overflow` explicitly.
  - Holds for / re-check: C++20 and later for (B); C++17 wording (N4659 [conv.integral] p3) checked for the older rule. UBSan run with Apple clang 21 (arm64), and on clang 18.1.3 and GCC 13.3 (Linux aarch64). Which checks `-fsanitize=undefined` enables differs per compiler; re-run the probe on yours.
</details>

### Card M7-PB26 · Recall
Q: Your crash dump shows a local `int` holding `0xaaaaaaaa` on one build and `0xfefefefe` on another; both builds use `-ftrivial-auto-var-init=pattern` (the compiler writes a pattern into every automatic variable that has no initializer). What explains it, and why should a test never assert on the pattern value? Run with no arguments, on each compiler you have:
   ```sh
   clang++ -std=c++20 -O0 -ftrivial-auto-var-init=pattern modules/07-architecture/lab/practice/autoinit_pick.cpp -o "${TMPDIR:-/tmp}/autoinit_pick" && "${TMPDIR:-/tmp}/autoinit_pick"
   ```
   Answer format: the explanation with both values, the documentation point, then the rule.
<details><summary>Answer</summary>
Key:

  - The pattern is compiler-specific: Apple clang 21 `=pattern` prints `stack value 0xaaaaaaaa`; GCC 13.3 `=pattern` prints `0xfefefefe`.
  - GCC's docs say the current byte is 0xFE and that the pattern may change in the future, so it is not a contract.
  - Use the pattern to make bugs visible (it is an unlikely value), not as a value to test for.
  - Holds for / re-check: Apple clang 21.0.0 arm64, GCC 13.3 aarch64, `-O0`. Re-check with the same one-line run on a new compiler; read the compiler's docs for `pattern` (gcc.gnu.org, Optimize-Options).
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
