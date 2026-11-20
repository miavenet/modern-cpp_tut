# Lesson 05 — Practice Bank (optional): What Moves to Build Time, and What a Conversion Costs

**Time: about 88 minutes** for 12 cards (4 + 6 + 5 + 8 + 12 + 12 + 4 + 6 + 8 + 10 + 6 + 7 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a table built by the compiler that gives 30 wrong answers the moment its limits change, and a `float` column that turns row 16777217 into row 16777216.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; diagnostics and listings can change with any compiler release.

## Cards

### Card M2-PB1 · Spot the trap
Q: A reviewer wants the multiply "removed at compile time" and writes:
   ```cpp
   int scale(int qty, bool big) {
     if constexpr (big) return qty * 100;
     return qty;
   }
   ```
   What happens when you compile it with `-std=c++20`, why, and what is the smallest fix that keeps the build-time choice? Check both:
   ```sh
   clang++ -std=c++20 -Wall -Wextra -fsyntax-only modules/02-type-system/lab/practice/ifc.cpp
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/ifc_fix.cpp -o "${TMPDIR:-/tmp}/ifc_fix" && "${TMPDIR:-/tmp}/ifc_fix"; echo "exit $?"
   ```
   Answer format: what the compiler says, the rule, then the fix.
<details><summary>Answer</summary>
Key:

  - It does not compile. Apple clang 21 and clang 18: `constexpr if condition is not a constant expression` (clang adds: function parameter `big` with unknown value). GCC 13.3: `'big' is not a constant expression`.
  - Rule: the condition of `if constexpr` must be a constant expression of type bool (C++20 draft N4861, 8.5.1 [stmt.if] p2). A function parameter is not one, even if every caller passes a literal.
  - **Fix:** make the choice a template argument: `template <bool Big> int scale(int qty)` and call `scale<true>(3)`. That compiles and returns 300 (`ifc_fix.cpp`, exit 0). If the value is only known at run time, a plain `if` is the honest code.
  - Holds for / re-check: a language rule since C++17, checked with Apple clang 21, and with clang 18.1.3 and GCC 13.3 on Ubuntu 24.04 aarch64, in C++20 mode. Re-check by compiling `ifc.cpp` on a new compiler; the error must remain.
</details>

### Card M2-PB2 · Spot the trap
Q: `discard.cpp` has two functions with `if constexpr`. `key_t<T>` is a template that calls `x.key()` only if `requires { x.key(); }` holds. `key_nt(const Plain&)` is a plain function with `constexpr bool has_key = false; if constexpr (has_key) return x.key();`. `Plain` has no `key()`. Which function compiles? Build it as is, then without `key_nt`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/discard.cpp -o "${TMPDIR:-/tmp}/discard"
   clang++ -std=c++20 -O2 -Wall -Wextra -DSKIP_NT modules/02-type-system/lab/practice/discard.cpp -o "${TMPDIR:-/tmp}/discard" && "${TMPDIR:-/tmp}/discard"
   ```
   Then try the two templates in `fixed_false.cpp` and `fixed_false_nondep.cpp` (both use `if constexpr (false)`):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/fixed_false.cpp -o "${TMPDIR:-/tmp}/fixed_false" && "${TMPDIR:-/tmp}/fixed_false"
   clang++ -std=c++20 -fsyntax-only modules/02-type-system/lab/practice/fixed_false_nondep.cpp
   ```
   Answer format: which function compiles, the rule in one line, then the fix.
<details><summary>Answer</summary>
Key:

  - `key_t<Plain>` compiles and returns `x.id` (prints `key_t=7`).
  - `key_nt` fails on both compilers: Apple clang "no member named 'key' in 'Plain'"; GCC 13.3 "'const struct Plain' has no member named 'key'".
  - Rule: when a templated entity is instantiated, a discarded statement is not instantiated if the condition is no longer value-dependent (C++23 draft N4950, 8.5.2 [stmt.if] p2). The operand has to depend on the template: `x.key()` with `x` of type `T` is checked only at instantiation. Outside a template, or on a non-dependent operand inside one, the discarded branch is checked at definition.
  - The condition itself need not depend on `T`: `if constexpr (false) return x.key();` with `const T& x` compiled and printed `fixed_false=7` on both compilers, while `Plain p; if constexpr (false) return p.key();` in a template is still an error.
  - **Fix:** put the operation on a template-dependent object in a template (or a generic lambda).
  - Holds for / re-check: `if constexpr` is C++17; the `requires` expression in `key_t` needs C++20 (GCC 13.3 `-std=c++17` rejects it). Checked with Apple clang 21 and GCC 13.3 in C++20 mode. Re-check by building the files on your compiler.
</details>

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

### Card M2-PB4 · Apply
Q: A long-running event counter is bumped twice per event and may run for years. You want proof, not an argument, that the type you picked can wrap safely. `bump_twice` is a `constexpr` function that adds 1 twice. Predict which of these compiles: `static_assert(bump_twice<std::uint32_t>(UINT32_MAX) == 1u);` and `static_assert(bump_twice<int>(INT_MAX) == INT_MIN + 1);` (the second only with `-DSIGNED`). Then check:
   ```sh
   clang++ -std=c++20 -fsyntax-only modules/02-type-system/lab/practice/wrap_constexpr.cpp
   clang++ -std=c++20 -fsyntax-only -DSIGNED modules/02-type-system/lab/practice/wrap_constexpr.cpp
   ```
   Answer format: which assert compiles, why the other does not, then how to use this.
<details><summary>Answer</summary>
Key:

  - The `uint32_t` assert compiles: unsigned arithmetic is modulo 2^N ([basic.fundamental]).
  - The `int` assert is rejected by both compilers. Clang: "static assertion expression is not an integral constant expression ... value 2147483648 is outside the range of representable values of type 'int'". GCC 13.3: "overflow in constant expression".
  - Why: signed overflow is undefined ([expr.pre] p4), and an expression whose evaluation would have undefined behaviour is not a core constant expression ([expr.const] p5.8). So constant evaluation is a free UB detector for small arithmetic functions.
  - Use: put wrap-sensitive arithmetic in a `constexpr` function and test its edge cases in `static_assert`s.
  - Holds for / re-check: any conforming C++20 compiler; checked with Apple clang 21 and GCC 13.3. It only covers the cases you assert; run-time paths still need UBSan.
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

### Card M2-PB7 · Recall
Q: In the C++26 working draft, a local `int a;` and a local `int b [[indeterminate]];` are both read before any write, and so is an `int` obtained with `new int`. For each read, is the behaviour undefined, erroneous, or fine? What is an implementation allowed to do on erroneous behaviour? Look it up in [basic.indet] and [defns.erroneous].
   Answer format: one word per read, then what the implementation may do.
<details><summary>Answer</summary>
Key:

  - `a`: erroneous behaviour. Its bytes start with "erroneous values ... determined by the implementation independently of the state of the program".
  - `b` and the `new int`: undefined behaviour. Dynamic storage and variables marked `[[indeterminate]]` start with indeterminate values.
  - Erroneous behaviour is "well-defined behavior that the implementation is recommended to diagnose"; it "is permitted to issue a diagnostic and is permitted to terminate the execution".
  - So `[[indeterminate]]` is an opt-out that gives back UB for that one variable.
  - Holds for / re-check: C++26 working draft N5054 (2026-07). Earlier standards (C++23 and before) make all three undefined. Re-check against the published standard when it appears ([basic.indet]).
</details>

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

### Card M2-PB10 · Apply
Q: `gcd_fold.cpp` computes `gcd(1071, 462)` with a recursive function four ways: `ask_optimizer` (plain static function, literal arguments), `ask_constexpr_fn` (same body declared `constexpr`, called in a `return` statement), `ask_forced` (the `constexpr` function used to initialise a `constexpr unsigned r`), and `ask_runtime(a, b)`. For each, predict: is a recursive call left? Is a divide left? Does the function just return 21? Check with `-O2 -S` on two compilers (GCC is `g++` on Linux):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/02-type-system/lab/practice/gcd_fold.cpp -o -
   g++ -std=c++20 -O2 -Wall -Wextra -S modules/02-type-system/lab/practice/gcd_fold.cpp -o -
   ```
   Answer format: recursion yes/no, then per function: divide left or returns 21, by compiler; then what would make "the compiler folds it" wrong.
<details><summary>Answer</summary>
Key:

  - No function keeps a recursive call: tail recursion became a loop everywhere. Recursion is no barrier to optimization.
  - `ask_optimizer`: a divide loop remains on all five toolchains (Apple clang 21, clang 18, GCC 13.3, gcc 14.3, clang 19.1) at -O2, and also at -O3 on the three Arm toolchains. Literal inputs did NOT make the compilers fold it.
  - `ask_constexpr_fn`: GCC (13.3 and 14.3) returns 21; the clangs still run the loop. Marking a function `constexpr` allows compile-time evaluation but does not require it in a run-time context.
  - `ask_forced`: returns 21 with no divide on all five toolchains. The initializer of a `constexpr` variable is required to be a constant expression ([dcl.constexpr] p10). Returning 21 without a divide is the observed code generation on these toolchains, not an ISO guarantee about instruction selection.
  - What would make a "the compiler folds it" claim wrong: a different compiler, -O level or version, or a loop the optimizer gives up on. If you need the value at compile time, force it (`constexpr` variable, `consteval`, template argument) instead of hoping.
  - Holds for / re-check: Apple clang 21 (arm64), GCC 13.3 and clang 18.1.3 (aarch64 Linux), gcc 14.3 and clang 19.1 (x86-64, Compiler Explorer) at -O2; -O3 checked on the three Arm toolchains. Optimizer folding limits change between releases; the requirement that a `constexpr` initializer be a constant expression does not. Re-check with `-S` and a grep for the divide (`udiv` on arm64, `div` on x86-64).
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

### Card M2-PB12 · Spot the trap
Q: A record contains bytes {A,B,C,D} and a pointer selecting its own B. A second record receives a `memcpy` of the first while both stay alive. The source's B is then changed to Z. Predict: is the pointer-bearing type trivially copyable and standard-layout? What are the destination's local byte and selected byte? Repeat with the selection stored as the integer offset 1, bounds-checked before indexing:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/relocate_record.cpp -o "${TMPDIR:-/tmp}/relocate_record" && "${TMPDIR:-/tmp}/relocate_record"
   ```
   Answer format: the two trait values, local and selected byte for each version, then what the traits do not certify.
<details><summary>Answer</summary>
Key:

  - Both traits print 1 for the pointer-bearing record. Yet the copied pointer still selects the source byte: destination local=B, selected=Z, points_to_source=1.
  - The offset version selects B from its own copied bytes after the source becomes Z.
  - Trivial copying preserves this pointer value; it does not rebase an internal address. These traits alone do not certify a relocatable record format.
  - For this bounded byte-array design, use a validated offset resolved against the destination. That does not by itself establish a portable file format or an inter-process synchronization protocol.
  - Holds for / re-check: Apple clang 21, C++20. Both records stay alive, so no dangling pointer is dereferenced; this is a single-process relocation test, not a shared-memory concurrency test. Re-check offset units, bounds and format compatibility when the record changes.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
