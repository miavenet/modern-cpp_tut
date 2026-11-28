# Lesson 07 — Practice Bank, part 8 of 9 (optional): Summing Floats

**Time: about 29 minutes (estimate)** for 3 cards (12 + 5 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: reduction order, what `unseq` permits, and accumulator chains in llvm-mca.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

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

---

## Close

Next: part 9, `lesson-07-practice-bank-09-compiler-assumptions.md` (about 26 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
