# Lesson 03 — Type traits, if constexpr and fold expressions

One serializer, two kinds of message: plain structs whose bytes you can copy
into a local byte buffer, and classes needing explicit serialization. In C++98
that meant two overloads and a trick to choose between them. C++17 lets one
function body ask "what is `T`?" and instantiate the branch that fits. By the
end you can branch on a type
trait with `if constexpr`, reduce a parameter pack with a fold expression, and
combine this module's tools in a design.

**Time:** about 41 min.
**You need:** lessons 01 and 02 of this module; variadic templates (`typename... Args`) are explained below.

---

### Card M2-P3 · Pretest
Q: Guess, before reading on. A variadic template receives any number of
   arguments. Before C++17, how would you add them all up? What do you think
   C++17 added?
   Answer format: one or two sentences. A guess is fine.

<details><summary>Answer</summary>
Key:
  - Before C++17: recursion. One overload handles a single argument; the
    variadic overload adds its first argument to a call on the rest.
  - C++17: a *fold expression*, `(args + ...)`, which expands the whole pack in
    one expression.
</details>

---

### CONCEPT C3  Type traits and if constexpr: branch on what T is

The C++98 overload maze grows quickly when one algorithm handles several
types. A trait lets you ask a type question in the template body; `if
constexpr` lets the answer select which branch is instantiated.

**Words first.** A *type trait* is a template that answers a question about a
type while compiling. `std::is_integral<T>::value` is `true` or `false`;
`std::decay<T>::type` is a transformed type. They live in `<type_traits>`
(C++11). C++14 added `_t` shortcuts (`std::decay_t<T>`), and C++17 added `_v`
shortcuts (`std::is_integral_v<T>`).

```cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
struct Quote { std::int64_t price; std::int32_t qty; };   // plain bytes
struct Note  { std::string text; };                       // owns a string value; may allocate
int main() {
    std::printf("%d %d\n", std::is_integral_v<int>, std::is_integral_v<double>);
    std::printf("%d\n", std::is_same_v<std::decay_t<const int&>, int>);
    std::printf("%d\n", std::is_same_v<std::decay_t<char[8]>, char*>);
    std::printf("%d %d\n", std::is_trivially_copyable_v<Quote>,
                           std::is_trivially_copyable_v<Note>);
}
```

**Predict first:** the four output lines (`-std=c++17`).

<details><summary>Output</summary>

`1 0`, `1`, `1`, `1 0`. `std::decay_t` removes a reference, turns an array
into a pointer (and a function into a function pointer), and otherwise drops
top-level `const`/`volatile`. A `std::string` member makes `Note` not
trivially copyable. Do not duplicate a live `Note` by copying its bytes into
another `Note`; use its copy constructor. Copying bytes into a byte buffer is
a different operation and does not serialize the string's contents.

</details>

**Branching on a trait.** In a template, an ordinary `if` still compiles both
branches for every `T`. If one branch calls `obj.customSerialize()` and `T` has
no such member, the build fails even though that branch would never run.
`if constexpr (condition)` (C++17) evaluates the condition while compiling and
*discards* the other branch for that instantiation. Both branches are parsed;
non-dependent errors can still be diagnosed.

This fragment demonstrates compile-time dispatch, not a portable wire format.
Assume `buffer` has sufficient writable storage. Trivial copyability alone
does not specify byte order, padding, pointer meaning, or a stable external
representation.

```cpp
template <typename T>
void serialize(const T& obj) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(buffer, &obj, sizeof(T));
    } else {
        obj.customSerialize(buffer);
    }
}
```

During template instantiation, the discarded branch is not instantiated for
that specialization. Outside a template, a discarded branch must still be
well-formed.

**Why C++11 code says `typename`.** Inside a template,
`std::enable_if<cond, int>::type` *depends* on `T`, so the compiler cannot know
whether `::type` names a type or a value. It assumes a value unless you write
`typename` in front. C++20 permits omission in type-only contexts, including
an alias declaration inside a function. An ordinary dependent local-variable
declaration such as `typename T::type value;` still needs it. The `_t` and
`_v` shortcuts avoid writing it.

**Rule:** traits answer questions about types at compile time, and
`if constexpr` instantiates only the selected branch for that `T`. Fine
print: both branches are parsed, and non-dependent errors can be diagnosed.

### Card M2-Q5 · Recall
Q: (a) What does the `<type_traits>` header give you?
   (b) Name two traits and say what each tells you or produces.
   Answer format: (a) one sentence; (b) two short lines.

<details><summary>Answer</summary>
Key:
  - (a) Compile-time templates that answer questions about types or transform
    them (results are constants or types, not run-time checks).
  - (b) Any two, for example: `std::is_integral_v<T>` (is `T` an integer type);
    `std::is_trivially_copyable_v<T>` (may its object representation be
    copied into bytes and back under the lifetime rules);
    `std::is_same_v<T, U>` (are they the same type); `std::decay_t<T>` (`T`
    without reference and top-level cv, arrays and functions turned into pointers).
</details>

### Card M2-Q6 · Apply
Q: This serializer fails to build when called with a plain struct `Quote`
   (which has no `customSerialize`):
   ```cpp
   template <typename T>
   void serialize(const T& obj) {
       if (std::is_trivially_copyable_v<T>) {
           memcpy(buffer, &obj, sizeof(T));
       } else {
           obj.customSerialize(buffer);
       }
   }
   ```
   (a) Why, when the condition is true for `Quote`?
   (b) What one-word change fixes it?
   Answer format: (a) one sentence; (b) the changed line.

<details><summary>Answer</summary>
Key:
  - (a) With an ordinary `if`, both branches are compiled for `T = Quote`, and
    the `else` branch calls a member `Quote` lacks (clang: `no member named
    'customSerialize' in 'Quote'`).
  - (b) `if constexpr (std::is_trivially_copyable_v<T>) {` The false branch is
    then discarded for `Quote`.
</details>

### Card M2-G8 · Spot the trap
Q: Given
   ```cpp
   template <typename T>
   auto getValue(T t) {
       if constexpr (std::is_pointer_v<T>) {
           return *t;
       } else {
           return "Not a pointer";
       }
   }
   int value = 5;
   ```
   (a) What does `getValue(&value)` return, and with what type?
   (b) Why is there no "two different return types" error?
   Answer format: one short line per part.

<details><summary>Answer</summary>
Key:
  - (a) `5`, as an `int`. (`getValue(42)` returns `"Not a pointer"` as a `const char*`.)
  - (b) For `T = int*` the `else` branch is discarded, so only `return *t;` takes
    part in deducing the return type. With a plain `if`, clang reports `'auto' in
    return type deduced as 'const char *' here but deduced as 'int'`.
</details>

### Card M2-G9 · Spot the trap
Q: Inside a function template body you write
   `std::enable_if<std::is_integral<T>::value, int>::type x = 0;`
   without `typename`. Clang says `missing 'typename' prior to dependent type
   name`. Why does the compiler need `typename` here?
   Answer format: one or two sentences.

<details><summary>Answer</summary>
Key:
  - `::type` depends on the template parameter `T`, so until `T` is known the
    compiler cannot tell whether it names a type or a value, and it assumes a
    value. `typename` says it is a type. `std::enable_if_t` (C++14) avoids
    writing it; this ordinary dependent local-variable declaration still
    requires it in C++20.
</details>

---

### CONCEPT C7  Fold expressions: reduce a parameter pack in one line

**A handful of overloads to add numbers.** In 2014 Andrew Sutton and Richard
Smith opened their fold proposal with the simplest job there is: add up some
arguments. With variadic templates, they wrote, "we have to resort to a handful
of overloads": one for no arguments, one for a single value, and one that adds
the first to the sum of the rest.

Their answer: "We should be able to do better." A fold puts the whole reduction
in one expression, and they argued that "programmers are much less likely to get
this wrong than the implementation above."

They also wanted to test a condition across every type in a pack, which is why
folds serve checks as well as sums. The judgement left to you is the empty case:
what should your operation mean for zero arguments?

**Words first.** A *parameter pack* (`typename... Args`, C++11) holds zero or
more template arguments; `args...` expands it.

**The C++11 way: recursion.**

```cpp
template <typename T>
T sum(T last) { return last; }
template <typename T, typename... Rest>
T sum(T first, Rest... rest) { return first + sum(rest...); }
// sum(1, 2, 3, 4) == 10
```

**The C++17 way: a fold expression** applies one operator across the pack:

| Written | Expands to (for a1, a2, a3) |
|---|---|
| `(args + ...)` | `a1 + (a2 + a3)` |
| `(... + args)` | `(a1 + a2) + a3` |
| `(0 + ... + args)` | `((0 + a1) + a2) + a3`, and `0` for an empty pack |

```cpp
#include <cstdio>
#include <iostream>
template <typename... Args> int right(Args... a) { return (a - ...); }
template <typename... Args> int left(Args... a)  { return (... - a); }
template <typename... Args>
void printAll(Args... args) { (std::cout << ... << args) << '\n'; }  // fold over <<
int main() {
    std::printf("%d %d\n", right(10, 3, 2), left(10, 3, 2));
    printAll("qty=", 100, " px=", 101.5);
}
```

**Predict first:** both lines of output.

<details><summary>Output</summary>

`9 5` (10 - (3 - 2) = 9; (10 - 3) - 2 = 5), then `qty=100 px=101.5`.

</details>

**The trap: an empty pack.** `(args + ...)` with no arguments has nothing to
expand. Clang: `unary fold expression has empty expansion for operator '+' with
no fallback value`. Only three operators have a value for an empty pack: `&&`
gives `true`, `||` gives `false`, and `,` gives `void()`. For anything else,
give an initial value: `(0 + ... + args)`.

**Rule:** a fold expression applies one operator across a pack; give it an
initial value unless the pack can never be empty or the operator is `&&`, `||` or `,`.

### Card M2-Q9 · Recall
Q: Which empty unary folds have built-in identities?
   Answer format: one line with their results.

<details><summary>Answer</summary>
Key:
  - Empty `&&` is `true`, empty `||` is `false`, and empty comma is
    `void()`. Other empty unary folds are ill-formed.
</details>

| Choice | What it does | Cost or obligation | Right when |
|---|---|---|---|
| Recursive overloads | Consume one argument per call | Base cases and overloads need review | Older standard is required |
| Unary fold `(args + ...)` | Combines a nonempty pack | Empty `+` pack is ill-formed | At least one argument is guaranteed |
| Binary fold `(0 + ... + args)` | Starts from an explicit identity | Identity must match the domain | Empty input should yield zero |

Advice: write the empty-case policy before choosing a fold; `sum()` failing
in an API that promises zero arguments is the smell.

### Card M2-J3 · Choose and explain
Q: The router passes one acknowledgement per venue it sent the order to, and
   marks the order live when `allConfirmed` returns true. Orders held
   internally are sent to no venue at all. An AI assistant wrote this fold.
   Is it right here? What would you check first, and what would change your
   answer? Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   template <class... Acks>
   bool allConfirmed(Acks... acks) { return (acks && ...); }
   int main() {
       std::printf("%d %d %d\n", allConfirmed(true, true), allConfirmed(true, false), allConfirmed());
   }
   ```

<details><summary>Answer</summary>
Key:
  - It is wrong here: an empty `&&` fold is `true`, so the program prints `1 0 1` and a held order that no venue confirmed would be marked live.
  - First check what an empty call means in this domain, then compile and run exactly that call.
  - If every call has at least one venue, or the question is really "did no venue reject it?", `true` for an empty pack is the right answer and the fold can stay.
</details>

### Card M2-C3 · Fix it
Q: Fill the three blanks:
   ```cpp
   template<typename... Args>
   void printAll(Args... args) {
       (std::cout << ... << ___) << '\n';      // blank 1: fold over <<
   }

   template <typename T>
   void maybeDereference(T val) {
       ___ (std::is_pointer_v<___>) {          // blank 2 (two words), blank 3
           std::cout << *val;
       } else {
           std::cout << val;
       }
   }
   ```
   Answer format: one line; three values in blank order, separated by commas.

<details><summary>Answer</summary>
Key:
  - `args, if constexpr, T`. With a plain `if`, `maybeDereference(42)` fails:
    `*val` is compiled for an `int`.
</details>

### Card M2-G7 · Spot the trap
Q: `template <typename... Args> auto sum(Args... args) { return (args + ...); }`
   (a) What happens when you call `sum()` with no arguments?
   (b) Change the fold so `sum()` returns 0.
   Answer format: (a) one sentence; (b) the new return statement.

<details><summary>Answer</summary>
Key:
  - (a) It does not compile: a unary fold over `+` has no value for an empty
    pack (only `&&`, `||` and `,` do).
  - (b) `return (0 + ... + args);`
</details>

---

## Synthesis: pull the module together

### Card M2-Q10 · Recall
Q: What can a compile-time constraint check, and what must still be checked
   when a message arrives at run time? Answer format: one sentence.

<details><summary>Answer</summary>
Key:
  - A constraint checks types and constant expressions during compilation;
    incoming values still need run-time validation.
</details>

### Card M2-INT1 · Choose and explain
Q: You inherit an order router: one `std::enable_if` overload of `route(T)` per
   order type, and a fee table filled by a loop at process start-up and read
   through a run-time `switch` on the order kind. Refactor it with C++20 tools.
   For each, say what it replaces and why it helps:
   (a) a named concept; (b) `if constexpr`; (c) a `constexpr` table.
   Answer format: one or two sentences per part.

<details><summary>Answer</summary>
Key:
  - (a) A concept such as `template <OrderMsg T> void route(const T&)` replaces
    the `enable_if` overload set: the requirement is named, and a wrong type is
    rejected at the call with the concept's name.
  - (b) `if constexpr (is_limit_v<T>) ... else if constexpr ...` replaces the
    per-type overloads inside one body: each instantiation uses only its
    own branch, so members that only some order types have are fine.
  - (c) A `constexpr` function builds the table, and `constexpr auto kFees =
    makeFees();` is computed by the compiler: no start-up loop, and constant
    initialization avoids start-up ordering problems. Indexing it by the order
    kind replaces the `switch`.
</details>

### Card M2-INT2 · Choose and explain
Q: An order message has fields `side`, `qty`, `price`, `tif`. You want a broken
   message *type* or a bad configuration constant to fail the build.
   (a) What would a concept check here?
   (b) What would a `consteval` function check?
   (c) What would a fold expression check?
   (d) Name one thing none of these can check.
   Answer format: one sentence per part.

<details><summary>Answer</summary>
Key:
  - (a) A concept `OrderMessage` requires `side()`, `qty()`, `price()`, `tif()`
    with convertible types; `template <OrderMessage T> void encode(const T&)`
    then rejects a type missing a field (`encode(Bad{})`: `'Bad' does not
    satisfy 'OrderMessage'`).
  - (b) A `consteval int checked_tick(int t) { if (t <= 0) throw "tick must be
    positive"; return t; }` checks a constant: `constexpr int kTick =
    checked_tick(0);` fails the build.
  - (c) A fold over the field types checks a property of every field in one
    `static_assert`, e.g. `((sizeof(Fields) <= 8) && ...)`.
  - (d) The values in a message received at run time (a zero `qty`, a price
    out of range, a truncated buffer): those still need run-time validation
    unless an earlier trusted stage already checked them.
</details>

## Remember

a trait asks about a type; `if constexpr` instantiates the selected branch
for that `T`. An empty unary fold has an identity only for `&&` (`true`),
`||` (`false`), and comma (`void()`). Other unary folds need a nonempty
pack; use a suitable binary fold when empty input is allowed. Compile-time
checks cover types and constants, not run-time
messages. Tomorrow the misses come back first. Next: module 03, concurrency.
