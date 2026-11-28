# Lesson 06 — Practice Bank, part 1 of 8 (optional): Compare-and-Swap on Real Hardware

**Time: about 26 minutes (estimate)** for 3 cards (12 + 4 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what each compare form compiles to, what a failed compare writes, and whether 16 bytes are lock-free.

Hook: Does asking for a weaker memory order change the instruction? Predict it per target before you look.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB1 · Apply
Q: Four tiny functions test whether an atomic 64-bit word equals `k`: acquire load + `==`, seq_cst load + `==`, relaxed compare-exchange that writes back `k`, seq_cst compare-exchange. Using only compiler builtins (no library headers; only the compiler's freestanding `<stdint.h>`), emit assembly for three targets from the one clang on your Mac. Then compile the same four functions written with `std::atomic` natively three times: no flag, `-march=armv8-a`, `-mcpu=apple-a7`. Predict first: on which targets does the memory order change the instruction?
   ```sh
   clang -O2 -S -ffreestanding --target=x86_64-linux-gnu modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang -O2 -S -ffreestanding --target=aarch64-linux-gnu -march=armv8.1-a modules/03-concurrency/lab/practice/atomic_probes_builtin.c -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_probes.cpp -o -
   ```
   (Repeat the last line with `-march=armv8-a`, then with `-mcpu=apple-a7`.)
   Answer format: one line per target, then your answer to the prediction.
<details><summary>Answer</summary>
Key:

  - x86-64: both loads are a plain `movq`; both CAS orders are the same `lock cmpxchgq`.
  - AArch64 (ARMv8.0): loads become `ldar` (acquire and seq_cst). The CAS is a load-exclusive / store-exclusive retry loop (`ldxr`/`stxr` relaxed, `ldaxr`/`stlxr` seq_cst). The order changes the CAS instructions, not the load.
  - AArch64 with ARMv8.1 atomics: the CAS is a single `cas` (relaxed) or `casal` (seq_cst); both loads are still `ldar`.
  - Native on this Mac (Apple clang default target): acquire load `ldapr`, seq_cst load `ldar`, relaxed CAS `cas`, seq_cst CAS `casal`. Adding `-march=armv8-a` changes nothing: the driver still passes `-target-cpu apple-m1` (check with `-###`), so the v8.1 atomics stay on. The lever is `-mcpu`: `-mcpu=apple-a7` gives `ldar` for both loads and retry loops (`ldxr`/`stxr` relaxed, `ldaxr`/`stlxr` seq_cst).
  - Answer to the prediction: CAS: the order changes the instruction on every AArch64 target (retry loop `ldxr/stxr` vs `ldaxr/stlxr`, or `cas` vs `casal`). Loads: acquire and seq_cst are the same `ldar` on ARMv8.0, ARMv8.1 and apple-a7; only the native apple-m1 target gives acquire its own `ldapr`. x86-64: no change for any of the four.
  - Lesson: a claim about what an atomic costs is a claim about one ISA and one target CPU. Read the instructions per target, and check which CPU the driver really targets.
  - Holds for / re-check: Apple clang 21 codegen and driver defaults. Other compilers or versions can pick other sequences and other default CPUs. Example: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), both compilers default to outline atomics (clang: `-target-cpu generic`, `+outline-atomics`), so the CAS is a call to a helper (`bl __aarch64_cas8_relax`, `bl __aarch64_cas8_acq_rel`), not an inline instruction; GCC with `-march=armv8.1-a` gives `cas`/`casal`. Re-check by re-emitting the same files with your compiler, `-march`/`-mcpu`, and `-###` to see the CPU the driver passes.
</details>

### Card M3-PB2 · Recall
Q: The C++ standard and the Intel manual describe a compare-exchange whose comparison fails in different terms. What does each say happens to memory, and why does the difference matter for "it failed, so nothing was written"? First attempt: look it up in the compare_exchange Effects of [atomics.types.operations] and the CMPXCHG page of the Intel SDM (325462-093); on reviews, recall it.
   Answer format: one line for C++, one for x86, one for why it matters.
<details><summary>Answer</summary>
Key:

  - C++: the standard keys this on the return value. If the operation returns true, it is an atomic read-modify-write; otherwise (including a spurious failure of the weak form, which can return false even when the values are equal) it is an atomic load, and `expected` gets the value read.
  - Intel SDM, CMPXCHG: the destination "receives a write cycle without regard to the result of the comparison"; on failure the old value is written back. "The processor never produces a locked read without also producing a locked write."
  - So the language-level meaning (a load) and the x86 hardware cost (a locked write to the line) differ. Use the language rules for correctness and the manual for cost.
  - Holds for / re-check: the C++ wording is in the N5054 draft and earlier drafts; the write-cycle sentence is Intel x86 (SDM 325462-093, CMPXCHG), and other ISAs use other instructions (see card M3-PB1). Re-check in the current SDM CMPXCHG page and [atomics.types.operations].
</details>

### Card M3-PB8 · Apply
Q: You want a 16-byte `{pointer, tag}` pair updated with one compare-and-swap, and a teammate says "`std::atomic<Tagged>` is lock-free on 64-bit machines." `wide_cas.cpp` prints `is_always_lock_free` and `is_lock_free()` for a 16-byte struct and contains one `compare_exchange_strong`. Check (a) the printed values with each compiler you have, and (b) which instruction or call the CAS becomes in the listing for each target:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos13 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   clang++ -std=c++20 -O2 --target=x86_64-apple-macos13 -march=x86-64 -S modules/03-concurrency/lab/practice/wide_cas.cpp -o -
   ```
   (Then add `-mcx16` to the last one.) On Linux, run the probe with Clang and GCC; on this Linux/libstdc++ setup, both GCC and Clang need `-latomic` for the executable probe (do not add it on the Mac):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -latomic -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
   g++ -std=c++20 -O2 -Wall -Wextra -DMAIN modules/03-concurrency/lab/practice/wide_cas.cpp -latomic -o "${TMPDIR:-/tmp}/wide_cas" && "${TMPDIR:-/tmp}/wide_cas"
   ```
   What do you find?
   Answer format: one line per toolchain or target, then what a `static_assert` on `is_always_lock_free` proves.
<details><summary>Answer</summary>
Key:

  - Apple clang 21, arm64 native: `is_always_lock_free` = 1, `is_lock_free()` = 1; the CAS is one `caspal` instruction.
  - Apple clang 21, `-target x86_64-apple-macos13`: `lock cmpxchg16b`. With `-march=x86-64` (a baseline without the 16-byte CAS feature) the same code becomes a call to `___atomic_compare_exchange_16`; adding `-mcx16` brings `lock cmpxchg16b` back.
  - GCC 13.3, aarch64 Linux (with `-latomic`): `is_always_lock_free` = 0, `is_lock_free()` = 0, and the CAS is a call to `__atomic_compare_exchange_16`.
  - clang 18.1.3 with libstdc++ on the same Linux (default flags plus `-latomic`, without which the link fails on `__atomic_is_lock_free`): `is_always_lock_free` = 1 but `is_lock_free()` = 0. The listing has no libatomic call for the CAS: it calls `__aarch64_cas16_acq_rel` (a helper that picks the instruction at run time), while `is_lock_free()` calls `__atomic_is_lock_free`. With `-march=armv8.1-a` the CAS is an inline `caspal`, and `is_lock_free()` still prints 0.
  - Per [atomics.types.operations] p4 and p5 (both normative), `is_always_lock_free` is true only if the type's operations are always lock-free, and `is_lock_free()` returns true if they are lock-free; Note 3 says the two are consistent. So the clang 18 + libstdc++ pair (1 and 0) cannot both be right: one of the two answers is wrong for that toolchain.
  - So `static_assert(std::atomic<T>::is_always_lock_free)` is necessary but not sufficient: here it passes while `is_lock_free()` says 0. Check both on the target, and read the listing (is the CAS an inline `cmpxchg16b`/`casp` or a call?).
  - Holds for / re-check: the toolchain/target combinations named above (Linux: Ubuntu 24.04 aarch64); x86 results come from Apple clang cross-compiling, not from an x86 run. They change with `-march`/`-mcpu`, library version, and compiler release. Re-check with `wide_cas.cpp` (`-DMAIN` to print) and `-S` on every new toolchain.
</details>

---

## Close

Next: part 2, `lesson-06-practice-bank-02-exchange-locks.md` (about 30 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
