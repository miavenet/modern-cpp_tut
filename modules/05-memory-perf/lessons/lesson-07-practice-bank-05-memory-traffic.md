# Lesson 07 — Practice Bank, part 5 of 9 (optional): Memory Traffic

**Time: about 27 minutes (estimate)** for 3 cards (3 + 12 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: cache-size boundaries in your own loop, 4K aliasing, and non-temporal stores.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

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

---

## Close

Next: part 6, `lesson-07-practice-bank-06-vectorization.md` (about 22 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
