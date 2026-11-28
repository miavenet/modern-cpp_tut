# Lesson 05 — Practice Bank, part 1 of 4 (optional): What Really Runs at Build Time

**Time: about 28 minutes (estimate)** for 4 cards (4 + 6 + 8 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: `constexpr` work, discarded `if constexpr` branches, and a compile-time proof that a counter is wide enough.

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

---

## Close

Next: part 2, `lesson-05-practice-bank-02-tables.md` (about 29 minutes). Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
