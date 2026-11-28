# Lesson 05 — Practice Bank, part 2 of 4 (optional): Tables the Compiler Builds

**Time: about 29 minutes (estimate)** for 3 cards (5 + 12 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: lookup tables computed at compile time, and floating-point template arguments.

Hook: A table built by the compiler becomes stale when a limit changes.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; diagnostics and listings can change with any compiler release.

## Cards

### Card M2-PB3 · Apply
Q: Someone says "a `double` can never be a template argument." Predict whether `template <double Factor> double scaled(double x)` compiles in C++17 and in C++20, then compile `nttp.cpp` in both modes with each compiler you have:
   ```sh
   clang++ -std=c++17 -O2 -Wall -Wextra modules/02-type-system/lab/practice/nttp.cpp -o "${TMPDIR:-/tmp}/nttp"
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/nttp.cpp -o "${TMPDIR:-/tmp}/nttp" && "${TMPDIR:-/tmp}/nttp"
   ```
   What do you see, and what does it teach about checking old advice?
   Answer format: one line per standard mode, the rule, then the lesson.
<details><summary>Answer</summary>
Key:

  - C++17: rejected by all three compilers tried (clang: "cannot have type 'double' before C++20"; GCC: "'double' is not a valid type for a template non-type parameter").
  - C++20: accepted by Apple clang 21, clang 18.1.3 and GCC 13.3; the program prints `1.50 3.00`.
  - Why: C++20 allows any *structural type* as a non-type template parameter, and scalar types (which include floating-point types) are structural (C++20 draft N4861: 13.2 [temp.param] p7, and 6.8 p9).
  - Lesson: advice about what the language forbids expires with the standard version; re-check with `-std=` set to the version you ship.
  - Holds for / re-check: C++20 and later; compiler support verified for the three compilers above only. Older compilers in C++20 mode may still reject it: test yours.
</details>

### Card M2-PB5 · Apply
Q: `tier_table.cpp` computes a discount tier from an order-size bucket (0 to 255) in two ways: `by_rule()` runs a ladder of comparisons against three limits; `by_table()` reads a 256-entry table that a `constexpr` function built from the same rule. (1) How do you prove in the source that the table was built at compile time? (2) Read both listings: what work is left in each? (3) The limits change at run time from {10, 50, 200} to {10, 80, 200}. Before the table is rebuilt, how many of the 256 answers are wrong?
   ```sh
   bash modules/02-type-system/lab/practice/tier_listing.sh clang++
   ```
   For (1), also try a table entry as a plain local array bound:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/tier_vla.cpp -o "${TMPDIR:-/tmp}/tier_vla" && "${TMPDIR:-/tmp}/tier_vla" 5
   clang++ -std=c++20 -O2 -Wall -Wextra -Werror=vla-cxx-extension -fsyntax-only modules/02-type-system/lab/practice/tier_vla.cpp
   ```
   Answer format: (1) the proof, (2) one line per function, (3) the count, then the price of the table.
<details><summary>Answer</summary>
Key:

  - (1) Declare it `constexpr auto FIXED = build(BOOT);`: the declaration is an error unless the initializer is a constant expression. Also `static_assert` on entries, or an entry as a template argument (e.g. `std::array<int, FIXED[10]>`). `constinit` also proves constant initialization but only applies to static or thread-storage objects.
  - An entry as a plain local array bound is NOT a proof on Clang: a non-constexpr value there compiled and ran (a variable-length-array extension, only a warning) unless `-Werror=vla-cxx-extension` is on.
  - (2) `by_table` is one indexed byte load and a return (GCC 13.3 and clang 18 add one instruction to widen the index). `by_rule` has three compares, plus selects or branches, on all three compilers.
  - (3) 30 answers are stale: buckets 50 to 79. After a rebuild, 0.
  - The comparisons now run when the table is built, not on each lookup. The price: each table covers the finite input domain and the limits used at its most recent construction (here built at compile time for `FIXED`, and rebuilt at run time for `live`), and it is wrong between a change and the next rebuild.
  - So a design with a table needs an owner for the rebuild and a rule for what the hot path does while the table is stale.
  - Holds for / re-check: the counts in (3) follow from the source. Listings checked with Apple clang 21, and clang 18.1.3 and GCC 13.3 on Ubuntu 24.04 aarch64, at `-O2`; x86 not checked. A compiler may turn a comparison ladder into something else (clang used conditional selects, GCC branches), so read the listing on your target. Nothing here says the table is faster in a real program: a table can miss the cache, and that must be measured in place.
</details>

### Card M2-PB6 · Apply
Q: `cls.cpp` builds a `constexpr std::array<Cls, 256>` that maps each byte to a character class (digit, sign, dot, space, other), filled by a constexpr function, with two `static_assert`s on its entries. `classify(unsigned char c)` returns `kCls[c]`. (a) `cls_bad.cpp` breaks one `static_assert` on purpose: when does the error appear? (b) What does the run print? (c) Look at `classify` in the `-O2` listing. Does "built at compile time" mean "free at run time"?
   ```sh
   clang++ -std=c++20 -fsyntax-only modules/02-type-system/lab/practice/cls_bad.cpp
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/cls.cpp -o "${TMPDIR:-/tmp}/cls" && "${TMPDIR:-/tmp}/cls" "-12.5" "+7x" " 3"
   clang++ -std=c++20 -O2 -S modules/02-type-system/lab/practice/cls.cpp -o -
   ```
   Answer format: (a), (b), (c), then what moved to build time and what did not.
<details><summary>Answer</summary>
Key:

  - (a) The broken assert stops the build: `static assertion failed due to requirement 'kCls['7'] == Other'`. The table is checked before the program exists.
  - (b) Output: `other 1 digit 5 sign 2 dot 1 space 1`.
  - (c) The table is in the binary as data, but `classify` is still one byte load indexed by the run-time character: `ldrb` (Apple clang 21, GCC 13.3, arm64) or `movzbl` (x86-64 GCC 14.3 and clang 19.1 on Compiler Explorer). No branches and no call.
  - So: the *table building* moved to build time; the *lookup* stays at run time because the input arrives at run time. The cost becomes one load, and that load can still miss the cache.
  - Holds for / re-check: checked on the four toolchains named; shapes may change with flags (for example `-O0`) or if the table grows. Re-check with `-S` on your target.
</details>

---

## Close

Next: part 3, `lesson-05-practice-bank-03-conversions.md` (about 20 minutes). Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
