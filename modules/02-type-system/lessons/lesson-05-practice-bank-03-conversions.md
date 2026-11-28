# Lesson 05 — Practice Bank, part 3 of 4 (optional): What a Conversion Costs

**Time: about 20 minutes (estimate)** for 3 cards (6 + 8 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: `float` round trips, double-to-integer instructions, and 128-bit products.

Hook: Can a floating-point column preserve every row number?

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; diagnostics and listings can change with any compiler release.

## Cards

### Card M2-PB8 · Apply
Q: A telemetry table keeps 32-bit row numbers in a `float` column "to save a conversion later". Predict what comes back from `static_cast<long>(static_cast<float>(row))` for row = 16777215, 16777216, 16777217, 16777219 and 1073741825, and the gap to the next float above 2^24 and above 2^30. Then run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/float_rows.cpp -o "${TMPDIR:-/tmp}/float_rows" && "${TMPDIR:-/tmp}/float_rows"
   ```
   Answer format: the values that change, the two gaps, then the fix.
<details><summary>Answer</summary>
Key:

  - `float` has 24 significand bits (`numeric_limits<float>::digits` = 24). Every integer up to 2^24 = 16777216 is exact.
  - 16777217 comes back as 16777216 (error -1); 16777219 as 16777220 (+1); 1073741825 as 1073741824 (-1). The gap is 2 above 2^24 and 128 above 2^30.
  - Two row numbers can become the same float, so lookups by the converted value silently merge rows. The standard allows either neighbour when the value is not exact ("an implementation-defined choice of either the next lower or higher representable value").
  - **Fix:** keep row numbers integral. If a floating type is unavoidable, check the range against `digits`.
  - Holds for / re-check: IEEE-754 binary32 `float` (true on mainstream targets; check `numeric_limits<float>::is_iec559`). Run on Apple clang 21, arm64. The rounding direction seen (to nearest) is the default mode; the standard leaves the choice to the implementation ([conv.fpint] p2).
</details>

### Card M2-PB9 · Apply
Q: `to_u64(double)` returns `static_cast<unsigned long>(d)`; `to_i64(double)` returns `static_cast<long>(d)`. Predict how many instructions each becomes, not counting the final `ret`, on (a) arm64, (b) x86-64 baseline, (c) x86-64 with `-mavx512f`. Build and count; then repeat (b) and (c) with GCC on Compiler Explorer.
   ```sh
   clang++ --target=arm64-apple-macos -std=c++20 -O2 -Wall -Wextra -S modules/02-type-system/lab/practice/to_unsigned.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu modules/02-type-system/lab/practice/to_unsigned.cpp -o -
   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu -mavx512f modules/02-type-system/lab/practice/to_unsigned.cpp -o -
   ```
   Answer format: a count (or shape) per target for unsigned and signed, then the habit.
<details><summary>Answer</summary>
Key:

  - arm64: one instruction each: `fcvtzu` (unsigned) and `fcvtzs` (signed).
  - x86-64 baseline: signed is one `cvttsd2si`. Unsigned is a 7-instruction straight-line sequence (Apple clang 21 and clang 18.1): two `cvttsd2si`, a `movq` copy, a `subsd` of a constant, `sarq $63`, `andq`, `orq`. Baseline x86-64 has no double-to-unsigned-64 instruction, so the compiler builds one.
  - GCC 13.3 and 15.3 x86-64 (Compiler Explorer, `-O2`): a different shape for the same cast: `comisd`, a conditional jump, and two return paths, with `subsd`, `cvttsd2siq` and `btcq` on the large-value path. Signed: one `cvttsd2siq`.
  - x86-64 with `-mavx512f`: one `vcvttsd2usi` for unsigned and one `vcvttsd2si` for signed (clang and GCC 13.3). The Intel SDM lists it under CPUID flag "AVX512F OR AVX10.1".
  - Habit: for any "it's just a cast" on a hot path, read the listing for the `-march` you really ship; signedness alone can change the code.
  - Holds for / re-check: arm64 and clang x86-64 from Apple clang 21 (clang 18.1 on Compiler Explorer gives the same 7); GCC 13.3 / 15.3 x86-64 on Compiler Explorer, compile only; nothing x86 was run. Counts exclude `ret`. Moving the build baseline to an AVX-512 or AVX10 target removes the sequence. Re-check with `-S` after a compiler or `-march` change.
</details>

### Card M2-PB11 · Spot the trap
Q: A geometry check squares `std::uint64_t x = (std::uint64_t{1} << 40) + 3` and stores the result in `unsigned __int128` (a 128-bit compiler-extension type, here called `Wide`). Compare `static_cast<Wide>(x*x)` with `static_cast<Wide>(x)*x`. Predict each one's high and low 64-bit halves, then run. Which matches the exact square?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/widen_product.cpp -o "${TMPDIR:-/tmp}/widen_product" && "${TMPDIR:-/tmp}/widen_product"
   ```
   Answer format: high and low for each cast, which one matches, then the rule.
<details><summary>Answer</summary>
Key:

  - Late cast: high=0, low=6597069766665. Early cast: high=65536, low=6597069766665.
  - Only the early cast matches the exact square, 2^80 + 6*2^40 + 9. Comparing the low halves alone would miss the error.
  - Widen an operand before multiplying: the `uint64_t` product wraps modulo 2^64, and casting the already-narrow result cannot recover the discarded high bits. This example uses unsigned arithmetic and does not rely on signed overflow.
  - Holds for / re-check: Apple clang 21, C++20 with the `unsigned __int128` extension. No division, timing or x86 instruction claim. Re-check that the extension exists on another compiler, and test both halves at operand boundaries.
</details>

---

## Close

Next: part 4, `lesson-05-practice-bank-04-object-bytes.md` (about 11 minutes). Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
