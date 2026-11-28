# Lesson 06 — Practice Bank, part 4 of 8 (optional): Plain, Volatile and Atomic Access

**Time: about 28 minutes (estimate)** for 3 cards (12 + 8 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: copying from a live source, a `volatile` ready flag, and stores the compiler may merge.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB9 · Apply
Q: You must copy 64 bytes from a live, correctly aligned source into a separate private destination. Another thread writes the source, using `std::atomic_ref` stores of the same type and granularity as each reader below, with no concurrent plain or differently sized access to those bytes. Three versions in `atomic_copies.cpp`: `copy_plain` (`memcpy`, a data race if a write overlaps), `copy_bytes` (each byte through `std::atomic_ref<unsigned char>` relaxed), `copy_words` (eight `std::atomic_ref<std::uint64_t>` relaxed loads on a `uint64_t[8]` struct). Predict how many load instructions each one executes, then check here and with x86-64 GCC and clang on Compiler Explorer:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_copies.cpp -o -
   ```
   Answer format: loads per version, then what the atomic versions do and do not give you.
<details><summary>Answer</summary>
Key:

  - `copy_plain`: wide vector loads. arm64: two `ldp q` load instructions (each loads two 16-byte registers, four 16-byte chunks in all); x86-64: four 16-byte loads (GCC `movdqu`, clang `movups`).
  - `copy_bytes`: 64 single-byte loads at run time. arm64 and x86 GCC: a loop with one `ldrb`/`movzbl` per iteration; x86 clang: the same loop unrolled by 4. No compiler merged the atomic byte loads.
  - `copy_words`: eight 8-byte loads executed (arm64 `ldr x`, unrolled; x86 clang `movq`, unrolled; x86 GCC `movq`, in an 8-iteration loop).
  - Lesson: atomic loads make each individual access race-free, and the listing shows their price; word-sized relaxed atomics cost far fewer loads than bytewise. This does not give a consistent snapshot: the copy can still mix values from different writes, so a seqlock-style check is still needed. No speedup is shown here, only instruction counts; measure before choosing, and check the listing again when the payload size changes.
  - Holds for / re-check: Apple clang 21 arm64, GCC 13.3 and clang 18.1 x86-64 (Compiler Explorer), `-O2`. `atomic_ref` needs the object alive, suitably aligned (`required_alignment`) and accessed only through `atomic_ref` while references exist; no subobject may be referenced concurrently by another `atomic_ref` ([atomics.ref.generic] p3, C++23 draft N4950), so mixing the byte and word versions on the same memory would be wrong. A future standard atomic memcpy, or a compiler that merges adjacent relaxed loads, would change this.
</details>

### Card M3-PB10 · Spot the trap
Q: In `volatile_flag.cpp` one thread fills a plain `Config` struct and then sets `volatile bool g_ready = true`; another spins `while (!g_ready) {}` and prints the struct. It prints the right values on your laptop. What is wrong, and which tool shows it without needing a lucky failure?
   ```sh
   clang++ -std=c++20 -O1 -g -fsanitize=thread modules/03-concurrency/lab/practice/volatile_flag.cpp -o "${TMPDIR:-/tmp}/volatile_flag" && "${TMPDIR:-/tmp}/volatile_flag"
   ```
   Answer format: what the tool reports, the rule, then the fix.
<details><summary>Answer</summary>
Key:

  - ThreadSanitizer reports two data races: on the flag (the spin line) and on the payload (the print line).
  - `volatile` gives neither atomicity nor inter-thread ordering. A non-atomic write and a concurrent read with no happens-before between them is a data race, which is undefined behaviour ([intro.races] p21, C++23 draft N4950).
  - "Prints the right values" proves nothing: correct output can be luck of the hardware and the compiler.
  - **Fix:** `std::atomic<bool>` with a release store and acquire load (or a mutex).
  - Holds for / re-check: any conforming compiler; the TSan report was seen with Apple clang 21 on arm64 macOS. TSan only finds races that the run executes, so a quiet run is not proof; a sanitizer can also miss races it does not model, such as fence-based code.
</details>

### Card M3-PB15 · Spot the trap
Q: `stage_stores.cpp` writes a "stage" variable three times in a row (1, then 2, then 3), in three versions: a plain `int`, a `volatile int`, and a `std::atomic<int>` with `memory_order_relaxed`. Two separate questions. (a) In the abstract machine, how many writes does each function perform that the standard requires to be preserved? (b) How many store instructions does each function actually contain at `-O1` and `-O2` with clang and with GCC (`g++` on Linux)? Then: a teammate says "volatile guarantees my debugger, signal handler or device sees 1, 2 and 3". Is that right?
   ```sh
   clang++ -std=c++20 -O1 -Wall -Wextra -S modules/03-concurrency/lab/practice/stage_stores.cpp -o -
   ```
   (Repeat with `-O2`, then with `g++`.)
   Answer format: (b) store counts for plain / volatile / atomic, then (a), then yes or no for the teammate with the reason.
<details><summary>Answer</summary>
Key:

  - (b) Observed: plain = 1 store (only the 3), volatile = 3, relaxed atomic = 3. Same on Apple clang 21, clang 18 and GCC 13.3 at -O1 and -O2 (arm64/aarch64), and at -O2 on gcc 14.3 and clang 19.1 x86-64.
  - (a) Abstract machine: accesses through volatile glvalues are part of the observable behaviour, so the program has three abstract volatile assignments ([intro.abstract]/6.1); what an access physically is is implementation-defined ([dcl.type.cv]/5). Say it as: "three abstract volatile assignments; three store instructions observed here". The standard does not specify a store-instruction count.
  - The plain count is allowed by the as-if rule: no other code can see the 1 and 2 without a data race.
  - The atomic count is a compiler choice. [intro.abstract]/6.1 names only volatile, and the standards committee has discussed why successive atomic updates may legally be combined (WG21 P0062R1). Today's compilers keep all three; do not build a "progress signal" on that.
  - The teammate is wrong. Preserved volatile accesses do not make a debugger stop at each one, do not stop a signal handler running only after all three writes, and a device needs platform-specific access rules. For a flag shared with a same-thread signal handler, the standard's type is `volatile std::sig_atomic_t` ([intro.races]/22, with its restrictions). To observe every stage you need a protocol (for example, the observer acknowledges each stage), not a store count.
  - Holds for / re-check: Apple clang 21 (arm64), GCC 13.3 and clang 18.1.3 (aarch64 Linux) at -O1 and -O2; gcc 14.3 and clang 19.1 (x86-64, Compiler Explorer) at -O2. The atomic count is a compiler choice and could change in a future release. Re-check by counting `str`/`stlr` (arm64) or `mov` to the variable (x86) in `-S` output.
</details>

---

## Close

Next: part 5, `lesson-06-practice-bank-05-threads-cores.md` (about 29 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
