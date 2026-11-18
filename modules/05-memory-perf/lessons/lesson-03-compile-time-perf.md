# Lesson 03 — Compile-Time Work, Branch Hints and `noexcept`

**Time: about 20 minutes** (reading about 11, cards about 9; running the samples is optional extra). Stop after any card; your place is saved.

**Hook:** the hash of `"AAPL"` below never runs in the program at all. Compile it, look at the assembly, and you find the finished number, `2001436`, sitting in the data section. This lesson shows when the compiler *must* do the work, when it merely *may*, and two hints whose effect you can only see by looking.

**You will be able to:** choose between `constexpr` and `consteval`, use `[[likely]]`/`[[unlikely]]` honestly, and predict what `noexcept` does when an exception escapes.

**Needs from earlier:** module 02's `constexpr` basics.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### Card M5-P3 · Pretest
Q: A function hashes symbol names such as `"AAPL"`. In C++98 it runs while the program runs. Guess one C++20 way to make sure the hash is computed by the compiler instead, so the running program only uses the result.
<details><summary>Answer</summary>
Key:

  - Mark the function `consteval` (C++20): every call must then be evaluated by the compiler, or the program does not compile.
  - Or keep it `constexpr` and store the result in a `constexpr` variable, which forces that one call to be evaluated at compile time.
  - Template metaprogramming and macros were the C++98 routes; any guess is fine here.
</details>

---

## Must vs may: `consteval` and `constexpr`

### CONCEPT C6  `constexpr` may run at compile time; `consteval` must

**Why `constexpr` needed a stricter sibling.** `consteval` came out of the committee's compile-time reflection work: reflection functions typically have to be evaluated at compile time. `constexpr` could permit compile-time evaluation but could not require every ordinary caller to use it. `consteval` supplied that missing contract. For your hash function, the decision is the same: is a run-time caller legitimate, or should it be rejected?

**Words first.** A *constant expression* is one the compiler can evaluate while compiling. A `constexpr` function (C++11) *can* be evaluated that way when its arguments are constants, and is an ordinary function otherwise. A `consteval` function (C++20, an *immediate function*) *must* be: a call the compiler cannot evaluate is a compile error.

**C++98 way vs modern way.** C++98 needed template tricks such as `template<int N> struct Fact { enum { value = N * Fact<N-1>::value }; };`. Modern C++ writes a normal function.

**See it yourself** (`clang++ -std=c++20 -Wall -Wextra ct.cpp && ./a.out 1 2`):

```cpp
#include <cstdint>
#include <cstdio>

consteval std::uint64_t hash_symbol(const char* str) {
    std::uint64_t hash = 0;
    while (*str) hash = hash * 31 + static_cast<unsigned char>(*str++);
    return hash;
}

constexpr int square(int x) { return x * x; }

std::uint64_t aapl_hash = hash_symbol("AAPL");   // computed by the compiler

int main(int argc, char**) {
    constexpr int a = square(12);   // constexpr variable: must be compile time
    int b = square(argc);           // argc is known only at run time
    std::printf("%llu %d %d\n", (unsigned long long)aapl_hash, a, b);
}
```

Predict the output with two command-line arguments (so `argc` is 3).

<details><summary>Real output</summary>

```text
2001436 144 9
```

`clang++ -std=c++20 -S ct.cpp -o -` shows `_aapl_hash: .quad 2001436` and no code for `hash_symbol` at all. `square(argc)` is an ordinary call: no constant-evaluation is required there (the optimiser may still fold calls whose arguments it can see).
</details>

**The wrong belief:** "`constexpr` on a function means it runs at compile time." Only when the call is in a context that requires a constant, such as a `constexpr` variable. Fine print: the initialiser of a static or thread-local variable, or of a `const` integer variable, is also evaluated at compile time when it is a constant expression.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `constexpr` function | Can serve constant and run-time arguments | One function serves both kinds of caller | You assume every ordinary call vanished |
| `consteval` function | Every usable call needs a constant result; run-time inputs fail to compile | A value must be fixed during compilation | A legitimate run-time caller can no longer compile |

Use `consteval` when a run-time call would be a bug; otherwise use `constexpr` and inspect the call site.

**Forcing it, and the error you get.** `int val = 5; constexpr int r = square(val);` fails with `constexpr variable 'r' must be initialized by a constant expression` and the note `read of non-const variable 'val' is not allowed in a constant expression`. If `square` were marked `consteval`, calling `square(argc)` would fail with `call to consteval function 'square' is not a constant expression`.

**Rule:** `consteval` guarantees compile-time evaluation for every call; `constexpr` guarantees it only where the context demands a constant (or for a constant initialiser of a static or `const` integer variable).


### Card M5-Q5 · Recall
Q: In one sentence each: (a) when is a call to a `constexpr` function evaluated at compile time for certain? (b) what does `consteval` change?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) When the call appears where a constant expression is required (for example, initialising a `constexpr` variable); otherwise it is an ordinary call that may run at run time. Fine print: a static or `const` integer variable with a constant initialiser is also initialised at compile time.
  - (b) Every call to a `consteval` function must be evaluated at compile time; if it cannot be, the program does not compile.
</details>

### Card M5-G5 · Spot the trap
Q: Your feed handler hashes symbols in two places: known symbols written in the source, and symbols read from a config file once at startup. An AI assistant changed `constexpr` to `consteval`, saying it "makes the hash free at run time" (fragment, needs `<cstdint>` and `<string>`). Is the change right here? What would you check first, and what would change your verdict? Give three short labeled parts.
   ```cpp
   consteval std::uint64_t hash_symbol(const char* s) {   // was constexpr
       std::uint64_t h = 0;
       while (*s) h = h * 31 + static_cast<unsigned char>(*s++);
       return h;
   }
   constexpr std::uint64_t kAAPL = hash_symbol("AAPL");   // symbol known in source
   bool isAAPL(std::uint64_t h) { return h == kAAPL; }
   std::uint64_t fromConfig(const std::string& sym) {     // runs once at startup
       return hash_symbol(sym.c_str());
   }
   ```
   Answer format: one line; separate (a), (b), and (c) with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) No: `kAAPL` was already a compile-time value under `constexpr`, and the run-time config call now fails to compile ("call to consteval function 'hash_symbol' is not a constant expression").
  - (b) First check every caller and whether each one has its input while compiling.
  - (c) If every caller hashed a literal, `consteval` would be the right promise; with a startup-only run-time caller, keep `constexpr`, since that call is not on the order path.
</details>

---

## Branch hints

### CONCEPT C7  `[[likely]]` and `[[unlikely]]`: a hint, not a command

**Why hints exist.** The compiler cannot always infer the production frequency of a branch from its source. A hint lets you supply that missing expectation. It still cannot promise a particular layout or speed-up.

**Words first.** An *attribute* is an annotation in `[[double brackets]]`. C++20 added `[[likely]]` and `[[unlikely]]`, placed on a branch to say "this path is much more (or less) likely". The standard's note says they are intended to let the compiler optimise for the likely path. What the compiler does with that (usually where it places the code) is its choice.

**C++98 way vs modern way.** Before C++20 you used a compiler extension such as GCC and Clang's `__builtin_expect(cond, 1)`. Now it is standard syntax:

```cpp
#include <cstdio>

int main(int argc, char**) {
    if (argc > 5) [[unlikely]] {
        std::puts("rare");
    } else [[likely]] {
        std::puts("common");
    }
    std::printf("%ld\n", (long)__has_cpp_attribute(likely));
}
```

<details><summary>Real output (no arguments)</summary>

```text
common
201803
```

A non-zero result reports support for the attribute. It does not prove that this build changes code generation: Clang ignores likelihood hints at `-O0` and when using profile-guided optimization.
</details>

**The wrong belief:** "`[[likely]]` makes the CPU predict better." The hint goes to the compiler only. Inspect generated code to see whether this build uses it; measure to see whether it helps. Marking every branch likely gives no preference among the marked branches, but unmarked paths can still differ.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| No hint | No source-level frequency claim | Frequency is unknown or unstable | A measured rare error path dominates layout costs |
| `[[unlikely]]` on an error path | States an expectation; compiler may use it | Error is genuinely rare in the measured workload | The hinted path is common in production traces |

Start with the measured branch frequency; reject a hint when real traffic contradicts it.

**Rule:** mark only the clear outliers (error paths), then check the assembly or measure.


### Card M5-C3 · Fix it
Q: Fill in the blanks: the first makes every call to the function compile-time only (C++20), the second hints that the branch is rare. Answer on one line: the two blanks in order, separated by a comma.
   ```cpp
// Fragment: fill the blanks in this exercise.
   ___ int compute_max_size() { return 1024 * 1024; }

   if (error_occurred) ___ {
       handle_error();
   }
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - (1) `consteval` (`constexpr` allows run-time calls, so it does not meet the first requirement)
  - (2) `[[unlikely]]`, written with the brackets
</details>

### Card M5-Q6 · Apply
Q: A developer puts `[[likely]]` on every `case` of a large `switch`, "to make the branch predictor faster". In two sentences: what is wrong with the reasoning, and with marking every case?
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - The attribute is a hint to the compiler (typically affecting code placement), not an instruction to the CPU's predictor, and its effect has to be checked in the generated code or measured.
  - Marking every case gives no relative preference among those cases. It can still distinguish them from an unannotated default or no-match path, so check which paths the switch actually has.
</details>

---

## `noexcept`

### CONCEPT C8  `noexcept` is a promise, enforced by `std::terminate`

**Why the promise matters.** A container moving elements during growth needs to know whether a move can fail. Marking a genuinely non-throwing move gives it useful information. Marking a function that may let `bad_alloc` escape instead turns that failure into program termination.

**Words first.** `noexcept` (C++11) declares that a function does not let exceptions out. During growth, a vector can copy existing elements when their move might throw and copying is available. For a move-only type it may have to use the potentially throwing move; if that throws, the usual strong exception guarantee may not apply. The C++98 spelling was `throw()`, a dynamic exception specification (deprecated since C++11; use `noexcept`).

**What if the promise is broken?** If an exception tries to leave a `noexcept` function, `std::terminate` is called; whether the stack is unwound first is implementation-defined. An exception caught *inside* the function is fine.

```cpp
#include <cstdio>
#include <new>

void safe() noexcept {
    try { throw std::bad_alloc(); }
    catch (const std::bad_alloc&) { std::puts("caught inside: fine"); }
}
void risky() noexcept { throw std::bad_alloc(); }   // escapes

int main() {
    safe();
    std::fflush(stdout);
    risky();
    std::puts("never printed");
}
```

<details><summary>Real output (exit status 134, an abort)</summary>

```text
caught inside: fine
libc++abi: terminating due to uncaught exception of type std::bad_alloc: std::bad_alloc
```

Clang also warned at compile time: `'risky' has a non-throwing exception specification but can still throw [-Wexceptions]`. It only sees throws written directly in the function, not ones from calls such as `push_back`.
</details>

**Rule:** mark a function `noexcept` only if it truly cannot let an exception out, or if ending the program is acceptable when it does.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `noexcept` | Escaping exception calls `std::terminate`; containers can trust the promise | A move that only transfers non-throwing members | An allocation inside can fail and nobody catches it |
| No `noexcept` | Exceptions may reach the caller; a copyable vector element may be copied on growth | Failure must be reported to the caller | Growth copies expensive objects despite a safe move being possible |

Promise `noexcept` only after checking every operation that can escape the function.


### Card M5-G6 · Spot the trap
Q: A `noexcept` function calls `std::vector::push_back`, which throws `std::bad_alloc`, and nothing inside the function catches it. What happens? One sentence.
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - `std::terminate` is called and the program ends (it is implementation-defined whether the stack is unwound first); catching the exception inside the function would have avoided this.
</details>

---

## Close

**Remember:** `consteval` must, `constexpr` may; branch hints are checked in the assembly, not assumed; an exception leaving `noexcept` ends the program. **Tomorrow:** missed cards come back. **Next:** lesson 04, parallel algorithms and small conveniences.
