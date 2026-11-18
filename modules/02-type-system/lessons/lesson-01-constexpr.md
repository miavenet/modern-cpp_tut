# Lesson 01 — constexpr and consteval: let the compiler do the arithmetic

Module 02 moves work from run time to compile time: `constexpr` and `consteval`
(this lesson), concepts (lesson 02), type traits, `if constexpr` and fold
expressions (lesson 03). It assumes module 01 and C++98 templates.

Your gateway builds a fee table at start-up, and the first order of the day pays
for that loop. You could make the compiler build the table instead. By the end
you can say when a `constexpr` call is guaranteed to happen at compile time, and
when it quietly runs at run time.

**Time:** about 29 min (reading about 17, cards about 12). The LPA lens on `constinit` is its own short lesson 04, so this one fits one sitting.
**You need:** C++98 templates; `clang++` for the snippets (`clang++ -std=c++20 -Wall -Wextra file.cpp && ./a.out`).

---

### Card M2-P2 · Pretest
Q: Guess, before reading on. In C++98, how could you make the compiler (not the
   running program) compute a value such as 5 factorial? And in C++20, which
   keyword makes a function refuse to run at run time at all?
   Answer format: one or two sentences. A guess is fine.

<details><summary>Answer</summary>
Key:
  - C++98: template metaprogramming. A recursive class template such as
    `Factorial<N>` computes the value while the compiler instantiates templates.
  - C++20: `consteval`. Every call to a `consteval` function must be evaluated by
    the compiler; a call it cannot evaluate is a build error.
</details>

### CONCEPT C1  constexpr functions: normal code the compiler can run

**Contorting `bitset::all`.** In 2013 Richard Smith took one small library
function, `std::bitset<N>::all()`, and tried to make it `constexpr` under the
C++11 rules. The ordinary version was a few lines: `std::any_of` with a lambda,
then an `if`.

The compile-time version had to throw all of that away. "We are forced to
reimplement std::any_of", he wrote. Lambdas were forbidden, `for` loops too, and
"if is forbidden, so we must use ?:, &&, and || instead". What came out was a
recursive helper built from nested conditional expressions.

His complaint: such code is "contorted" to fit the rules, and the restrictions
cause "programmer frustration". C++14 let a `constexpr` function look like the
runtime function you would have written anyway. One question remains, at the
call site: is compile-time evaluation required there?

**The C++98 way.** A reusable C++98 compile-time factorial commonly used
recursive templates. Each `Factorial<N>` instantiates `Factorial<N-1>` until a
specialisation stops the recursion:

```cpp
template <int N>
struct Factorial { static const int value = N * Factorial<N - 1>::value; };
template <>
struct Factorial<0> { static const int value = 1; };
// Factorial<5>::value is 120, computed during compilation
```

**Why it hurts.** It is not a function. You cannot pass it a run-time value,
loop, or debug it, and mistakes produce template errors.

**The modern way.** A `constexpr` function is an ordinary function that the
compiler is *allowed* to run while compiling. The rules widened over time:

| Standard | What a `constexpr` function body may contain |
|---|---|
| C++11 | essentially one `return` statement (recursion and `?:` allowed) |
| C++14 | local variables, loops, `if`, several statements |
| C++20 | also `try` blocks, virtual calls, and `new`/`delete` if freed before the evaluation ends; so `std::vector` and `std::string` work inside one |

```cpp
constexpr int factorial11(int n) { return n <= 1 ? 1 : n * factorial11(n - 1); } // C++11
constexpr int factorial(int n) {                                                  // C++14
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}
static_assert(factorial(5) == 120, "checked by the compiler");
```

**Predict first:** the loop version, compiled with `-std=c++11`. Does it build?

<details><summary>Output</summary>

No. Apple clang 21 says `error: statement not allowed in constexpr function`
(the `for` loop), after `warning: variable declaration in a constexpr function
is a C++14 extension`. With `-std=c++14` it builds.

</details>

**The trap.** The C++20 row means memory allocated *during* the evaluation may
be used and freed there. A `constexpr std::vector<int> table{1, 2, 3};` variable
still fails (clang: `pointer to subobject of heap-allocated object is not a
constant expression`): the allocation would outlive the evaluation. An empty
`constexpr std::vector<int>` is permitted because it retains no allocation.

**Rule:** a `constexpr` function is normal C++ that the compiler may run; C++14
made loops legal and C++20 allowed temporary heap use inside the evaluation.

### Card M2-Q3 · Recall
Q: A C++20 `constexpr` function builds a local vector, sums its elements, and
   returns the sum. A reviewer rejects it because “compile-time code cannot
   allocate”. Is the rejection right, and what must happen to the allocation?
   Answer format: two short sentences.

<details><summary>Answer</summary>
Key:
  - No. Allocation is allowed during constant evaluation if it is released
    before that evaluation ends. Returning the sum can satisfy that condition;
    retaining the allocation cannot.
</details>

---

### CONCEPT C4  "May" versus "must": when the compiler really evaluates it

A function declaration alone cannot promise that a startup calculation
disappears. The call site must require a constant expression when that
property is part of the design.

`constexpr` on a function means **may** run at compile time. Whether it does
depends on where you call it.

A *constant expression* is an expression the compiler can fully evaluate
while compiling. Some places **require** one: a `constexpr` variable's
initialiser, an array bound, a template argument, `static_assert`. Elsewhere
constant evaluation is not required. Fine print: the compiler also tries it
for the initialiser of a static or thread-local variable, and of a `const`
integer variable.

| Declaration | Meaning |
|---|---|
| `const int y = x;` | read-only after initialisation; the value may come at run time |
| `constexpr int z = f();` | the initialiser must be a constant expression, or the build fails |

**See it yourself.** `std::is_constant_evaluated()` (C++20) tells you whether
the compiler is evaluating this call as a constant, under the language rules,
or whether it is running as ordinary code. In the example
below, the `constexpr` initializer makes it return true; the ordinary `int`
initializer makes it return false, even if optimization computes that result
early.

```cpp
#include <cstdio>
#include <type_traits>  // std::is_constant_evaluated is C++20

// Returns 1 in a manifestly constant-evaluated context, 0 otherwise.
constexpr int byCompiler() { return std::is_constant_evaluated() ? 1 : 0; }

int main() {
    int a = byCompiler();            // ordinary initialization
    constexpr int b = byCompiler();  // a constant expression is required
    std::printf("a=%d b=%d\n", a, b);
}
```

**Predict first:** what are `a` and `b`, at `-O0` and at `-O2`?

<details><summary>Output</summary>

`a=0 b=1` at both levels (Apple clang 21, arm64). The optimiser may still
compute an ordinary call early: here `parsePort("8080")` is called at `-O0`
(`bl __Z9parsePortPKc` in `clang++ -std=c++20 -O0 -S`) and folded to the
constant at `-O2`. That is an optimisation, not a promise.

```cpp
constexpr int parsePort(const char* s) {
    int port = 0;
    for (; *s; ++s) port = port * 10 + (*s - '0');
    return port;
}
int main() { int port = parsePort("8080"); return port == 8080 ? 0 : 1; }
```

</details>

**The trap.** "It is `constexpr`, so it is free at run time." Only in a context
that requires a constant expression. With run-time arguments, constant
evaluation is not required; an optimizer may still fold an ordinary call.

**Rule:** a `constexpr` call is guaranteed compile-time work only where a
constant expression is required, such as a `constexpr` variable.

### Card M2-G1 · Spot the trap
Q: What happens on the last line?
   ```cpp
   int x = getRuntimeValue();
   const int y = x;
   constexpr int z = x;   // this line
   ```
   Answer format: one sentence: does it compile, and why.

<details><summary>Answer</summary>
Key:
  - It does not compile: a `constexpr` variable needs a constant expression, and
    `x` is only known at run time (clang: `constexpr variable 'z' must be
    initialized by a constant expression`, `read of non-const variable 'x'`).
    The `const int y = x;` line is fine: `const` only means read-only.
</details>

### Card M2-Q4 · Apply
Q: `constexpr int parsePort(const char* str)` exists. You write
   `int port = parsePort("8080");`
   (a) Is this call guaranteed to be evaluated at compile time?
   (b) Without touching `parsePort`, change this one line so that it is guaranteed.
   Answer format: (a) yes or no plus a reason; (b) the new line of code.

<details><summary>Answer</summary>
Key:
  - (a) No. Initialising a plain `int` does not require a constant expression,
    so it is an ordinary call. The optimiser may fold it (Apple clang did at
    `-O2`, not at `-O0`), but nothing guarantees that.
  - (b) `constexpr int port = parsePort("8080");` A constant expression is now
    required, so the compiler must evaluate it or reject the build.
</details>

### Card M2-G2 · Spot the trap
Q: `constexpr int getConfigValue(int x)` shows up in your hot-path profile.
   Callers pass values read from the order. Why is it running at run time?
   Answer format: one or two sentences.

<details><summary>Answer</summary>
Key:
  - `constexpr` means the function *may* run at compile time. These calls use
    run-time arguments and are not in a context that requires a constant
    expression, so they are ordinary run-time calls. Marking it `consteval`
    would not make it faster: the calls would become build errors. Only values
    known while compiling can be moved to compile time.
</details>

---

### CONCEPT C5  consteval and constinit (C++20): turning "may" into "must"

**Why.** With `constexpr` a reader cannot tell whether a call happens at compile
time. C++20 added two keywords that make the build fail instead.

- `consteval` on a function makes it an *immediate function*: every call must
  be evaluated by the compiler. A call with a run-time argument is an error.
- `constinit` on a static or thread-local variable requires its initial value
  to be computed at compile time. The variable stays mutable afterwards.

```cpp
#include <array>
#include <cstdio>
consteval int getTableSize(int base) {
    int size = base;
    for (int i = 0; i < 4; ++i) { size *= 2; }
    return size;
}
std::array<int, getTableSize(8)> table;     // array of 128 ints

constexpr int limit() { return 1000; }
constinit int max_orders = limit();         // set before any code runs

int main() { max_orders += 1; std::printf("%zu %d\n", table.size(), max_orders); }
```

**Predict first:** what prints? And what happens to
`int main(int argc, char**) { return getTableSize(argc); }`?

<details><summary>Output</summary>

`128 1001`. The second program does not build: `call to consteval function
'getTableSize' is not a constant expression`, `function parameter 'argc' with
unknown value cannot be used in a constant expression`.

</details>

**The trap.** Inside a compile-time evaluation the compiler may not read a
mutable global's value. Passing that global's *address* is allowed; reading
through the pointer is not.

**Rule:** `consteval` says every call happens at compile time, and `constinit`
says a global's starting value is computed at compile time; both fail the build
otherwise. Lesson 04 shows what a `constinit` counter buys on a latency path.

| Tool | What it guarantees | Cost or obligation | Right when |
|---|---|---|---|
| `constexpr` function | Can be evaluated at compile time | Call site may still run at runtime | Same operation serves both kinds of input |
| `consteval` function | Every call is evaluated at compile time | Runtime argument is rejected | Runtime call would be a design error |
| `constinit` variable | Static or thread-local initialization is constant | Variable may change later | Mutable state needs a fixed starting value |

Advice: put `consteval` on a function only if every valid caller has
compile-time inputs; a runtime argument in real callers is the smell.

### Card M2-J1 · Choose and explain
Q: An admin thread may raise this order limit while the risk thread reads
   it. The AI assistant's comment says `constinit` makes it safe to share. Is
   the declaration right, and is the comment? What would you check first?
   Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   constexpr int defaultLimit() { return 1000; }
   // AI comment: "constinit: computed at compile time, so read-only and safe to share"
   constinit int maxOrders = defaultLimit();
   void adminRaise() { maxOrders += 500; }
   int main() {
       adminRaise();
       std::printf("%d\n", maxOrders);
   }
   ```

<details><summary>Answer</summary>
Key:
  - The declaration does its one job, a starting value fixed at compile time, but the comment is false: `constinit` does not make the variable read-only, and `adminRaise` compiles and prints `1500`.
  - First check who writes it after start-up: if nothing should, `constexpr int` makes the compiler reject the write.
  - Because the admin thread writes while the risk thread reads, a plain `int` here is a data race, so I would reject it until it is `constinit std::atomic<int>` or guarded by a lock.
</details>

### Card M2-C2 · Fix it
Q: Fill the blank so this C++20 function can never run at run time:
   ```cpp
   ___ int getTableSize(int base) {
       int size = base;
       for (int i = 0; i < 4; ++i) { size *= 2; }
       return size;
   }
   std::array<int, getTableSize(8)> table;
   ```
   Answer format: one keyword.

<details><summary>Answer</summary>
Key:
  - `consteval`. (`constexpr` would also build here, because an array bound
    requires a constant expression, but it would allow run-time calls elsewhere.)
</details>

### Card M2-G3 · Spot the trap
Q: Clang rejects this program. Is the problem passing `&global_val` to a
   `consteval` function, or something else?
   ```cpp
   int global_val = 5;
   consteval int compute(int* p) { return *p * 2; }
   int main() { return compute(&global_val); }
   ```
   Answer format: one or two sentences naming the operation that fails.

<details><summary>Answer</summary>
Key:
  - Something else: reading `*p`. The address of a global is a valid
    constant-expression value (a `consteval` function comparing
    `p == &global_val` builds), but reading the value of the non-const
    `global_val` during compile-time evaluation is not allowed (clang: `read of
    non-const variable 'global_val' is not allowed in a constant expression`).
    Declaring `constexpr int global_val = 5;` with a `const int*` parameter builds
    and returns 10.
</details>

## Remember

`constexpr` means *may*; a constant-expression context or
`consteval` means *must*. `constinit` fixes a global's starting value at compile
time. Tomorrow the misses come back first. Next: concepts, which put
requirements on template arguments.
