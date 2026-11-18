# Lesson 02 — Concepts: say what a template needs

Put an `Order` with no `operator<` into a `std::map` and clang prints 71 lines,
starting inside a libc++ header you never opened. A C++20 concept moves that
complaint to your call site and names the missing requirement. By the end you
can write a concept, constrain a template with it, and predict which of two
constrained overloads the compiler picks, or why it refuses to pick.

**Time:** about 35 min.
**You need:** lesson 01 of this module; C++98 function templates and overloading.

---

### Card M2-P1 · Pretest
Q: Guess, before reading on. Before C++20, how could you make a function
   template accept only integer types? And what does C++20 offer instead?
   Answer format: one or two sentences. A guess is fine.

<details><summary>Answer</summary>
Key:
  - C++11: `std::enable_if` with a trait such as `std::is_integral<T>`, which
    removes the template from overload resolution for other types (SFINAE).
    C++98 had no `std::enable_if`; people wrote their own helper or used Boost.
  - C++20: a *concept* in a `requires` clause, e.g. `template <typename T>
    requires std::integral<T>`, or the short form `void f(std::integral auto x)`.
</details>

---

### CONCEPT C2  From "accept anything" to named requirements

A concept gives a requirement a name, but naming the wrong requirement still
admits the wrong types. Before writing one, state what the template needs to
do. We will test that contract with both a number and a string.

**Words first.** A *constraint* is a compile-time true/false condition on a
template argument. A *concept* is a named constraint (C++20), such as
`std::integral<T>`. A *requires clause* attaches a constraint to a template. A
*requires expression*, `requires (T t) { ... }`, can check operations, types,
return-type requirements, and nested constraints. Concepts express compile-time
Boolean constraints.

**The C++98 way: no constraint.** A template accepts any type, and the error
appears wherever the body first fails. The first error for this example may
appear inside a library header:

```cpp
#include <map>
#include <string>
struct Order { std::string symbol; };
int main() {
    std::map<Order, int> book;   // Order has no operator<
    Order key;
    key.symbol = "AAPL";
    book[key] = 1;
}
```

**C++11: SFINAE.** "Substitution failure is not an error": if putting `T`
into a template's declaration produces an invalid type, that template is
silently dropped from the overload set. `std::enable_if` (C++11) exploits this:

```cpp
template <typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
process(T value);
```

It works, but the requirement is hidden inside the return type.

**C++20: concepts.** The requirement becomes a readable declaration:

```cpp
#include <concepts>
#include <cstdio>
template <typename T>
    requires std::integral<T>
void process(T value) { std::printf("integer %ld\n", static_cast<long>(value)); }

int main() {
    process(42);
    process(1.5);   // what does the compiler say?
}
```

**Predict first:** what does clang report for `process(1.5)`?

<details><summary>Output</summary>

```
error: no matching function for call to 'process'
note: candidate template ignored: constraints not satisfied [with T = double]
note: because 'double' does not satisfy 'integral'
```

Honest comparison: for this one-condition `enable_if` version, clang 21 is
also short (`requirement 'std::is_integral<double>::value' was not
satisfied`). The concept wins on reading and reuse, and its name appears in
the message.

</details>

**Short form.** `void process(std::integral auto value)` is an *abbreviated
function template*: a constrained `auto` *parameter* makes the function a
template, so it accepts `short`, `long`, `char` and the rest. A constrained
*variable*, `std::integral auto count = 3;`, is just an `int` checked once.

**The trap.** A named requirement can still be too weak. Requiring only `a + b`
admits strings as well as numbers: a string's `+` concatenates.

**Rule:** a concept names the operations a template needs, so a bad type fails
at the call site with the concept's name.

| Interface | What it checks | Cost or obligation | Right when |
|---|---|---|---|
| Unconstrained template | Body fails when an operation is missing | Error may surface deep in an instantiation | Small private generic helper |
| Concept-constrained template | Named compile-time requirements | You must specify the real contract | Public generic API with known operations |
| Virtual interface | Runtime implementation through a base type | Indirection and object model | Concrete type varies at runtime |

Advice: use a concept for a stable generic contract; a concept that accepts
the wrong meaning of an operation is a smell.

### Card M2-J2 · Choose and explain
Q: Version 1 of the trading engine links exactly one strategy, chosen at
   build time. Version 2 must pick strategies by name from a config file at
   start-up and keep them in one `std::vector`. An AI assistant wrote this
   concept-based design. Is it right for each version? What would you check
   first? Answer format: three short sentences.
   ```cpp
   #include <concepts>
   #include <cstdio>
   template <class S>
   concept Strategy = requires(S s, double px) { { s.onPrice(px) } -> std::same_as<int>; };
   struct Momentum { int onPrice(double px) { return px > 100 ? 1 : 0; } };
   template <Strategy S> int runTick(S& s, double px) { return s.onPrice(px); }
   int main() {
       Momentum m;
       std::printf("%d\n", runTick(m, 101.0));
   }
   ```

<details><summary>Answer</summary>
Key:
  - For version 1 it is right: the concept names the one operation `runTick` needs, the call is resolved at compile time, and this prints `1`.
  - First check when the concrete strategy type becomes known: while compiling, or only after the config file is read.
  - For version 2 the verdict changes, because a concept gives no single type that one vector can hold for different strategies, so a virtual `Strategy` base (or a `std::variant` over a closed set) is the right tool.
</details>

### Card M2-C1 · Fix it
Q: Fill the three blanks to define a concept `OrderType`: a type whose
   `price()` returns exactly `double`.
   ```cpp
// Fragment: fill the blanks in this exercise.
   template <typename T>
   ___ OrderType = ___ (T t) {
       { t.price() } -> std::same_as<___>;
   };
   ```
   Answer format: one line; three words in blank order, separated by commas.

<details><summary>Answer</summary>
Key:
  - `concept, requires, double`. With them, a type whose `price()` returns
    `double` satisfies `OrderType`; one returning `int` does not.
</details>

### Card M2-Q1 · Recall
Q: Give two advantages of a C++20 concept over a C++11 `std::enable_if`
   condition.
   Answer format: two short lines, (a) and (b).

<details><summary>Answer</summary>
Key:
  - (a) Readability and reuse: the requirement is a named declaration
    (`requires std::integral<T>`) instead of a trick hidden in the return type.
  - (b) Diagnostics name the concept that was not satisfied, at the call site
    (clang: `because 'double' does not satisfy 'integral'`).
</details>

### Card M2-G4 · Spot the trap
Q: `template <typename T> concept Addable = requires(T a, T b) { a + b; };`
   constrains `add` for numbers and price vectors. Then
   `add(std::string("BUY"), std::string("SELL"))` compiles and returns
   `BUYSELL`. Why did the concept let it through?
   Answer format: one sentence.

<details><summary>Answer</summary>
Key:
  - This particular `Addable` checks only that `a + b` is well-formed; it
    never asks whether `+` means numeric addition.
</details>

### Card M2-G6 · Spot the trap
Q: A function parameter is declared `const std::integral auto& val`. Does that
   restrict `val` to exactly `int`?
   Answer format: yes or no, and one sentence why.

<details><summary>Answer</summary>
Key:
  - No. A constrained `auto` parameter makes the function an abbreviated
    function template, so any type that satisfies `std::integral` is accepted
    (`short`, `long`, `char`, `unsigned`...), each giving its own instantiation.
</details>

---

### CONCEPT C6  Choosing between overloads: subsumption

Two overloads can both accept a type. C++20 then prefers the **more
constrained** one, but only when it can *prove* one constraint includes the
other.

**Words first.** An *atomic constraint* is one indivisible condition, such as
`sizeof(T) > 4`, at the place in the source where it is written. Two atomic
constraints are identical when they originate from the same expression and
their parameter mappings are equivalent. Constraint P *subsumes* Q when P being true
proves Q true, comparing atomic constraints by that identity.

For otherwise-equivalent viable function-template overloads, constraints can
break the tie as follows.

| Candidates | Result |
|---|---|
| constrained vs unconstrained | the constrained one wins |
| one constraint subsumes the other | the subsuming (more constrained) one wins |
| neither is more constrained | constraints do not break the tie; the call is ambiguous if no other rule distinguishes the candidates |

**See it yourself.** A feed handler with a generic path and a market-data path:

```cpp
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>

template <typename T>
concept Message = requires(const T& m) {
    { m.length() } -> std::convertible_to<std::size_t>;
};

// Built ON TOP of Message: Message<T> appears inside it.
template <typename T>
concept MarketDataMessage = Message<T> && requires(const T& m) {
    { m.seq() } -> std::convertible_to<std::uint64_t>;
};

void process(const Message auto&)           { std::puts("generic path"); }
void process(const MarketDataMessage auto&) { std::puts("market-data path"); }

struct Heartbeat { std::size_t length() const { return 0; } };
struct Trade {
    std::size_t length() const { return 32; }
    std::uint64_t seq() const { return 7; }
};

int main() {
    process(Heartbeat{});
    process(Trade{});
}
```

**Predict first:** what prints? Then: what if `MarketDataMessage` repeats the
`length()` requirement instead of writing `Message<T> &&`?

<details><summary>Output</summary>

`generic path`, then `market-data path`. `Trade` satisfies both; the second
concept contains `Message<T>`, so it subsumes `Message` and wins.

With the requirement copied, clang says `error: call to 'process' is
ambiguous`: the two `length()` checks are different atomic constraints.

</details>

This choice happens during compilation, so no run-time lookup is involved. It
only works when the message type is known at compile time; a queue of mixed
message types still needs run-time dispatch (a `virtual` call or a `switch`).

**Rule:** build the narrower concept *from* the wider one (`Wide<T> && ...`) so
the compiler can prove the ordering; restated conditions are unrelated.

### Card M2-Q8 · Recall
Q: When two otherwise-equivalent function templates both match, what must
   their constraints show for one to win? Answer format: one sentence.

<details><summary>Answer</summary>
Key:
  - One constraint must subsume the other: the compiler must prove it is more
    constrained using atomic constraints with matching origins and parameter
    mappings.
</details>

### Card M2-Q2 · Apply
Q: Two overloads exist:
   ```cpp
   template <typename T> void handle(T t);
   template <std::integral T> void handle(T t);
   ```
   (a) Which one does `handle(5)` call, and why?
   (b) Is that choice made at compile time or at run time?
   Answer format: one short line per part.

<details><summary>Answer</summary>
Key:
  - (a) The `std::integral` one: both accept `int`, and a constrained template
    is more constrained than an unconstrained one. (`handle(2.5)` calls the other.)
  - (b) At compile time, during overload resolution; nothing is looked up at run time.
</details>

### Card M2-C4 · Fix it
Q: Fill the three blanks so market-data messages take the fast path:
   ```cpp
   template <typename T>
   concept MarketDataMessage = ___<T> && requires(const T& m) {
       { m.seq() } -> std::convertible_to<uint64_t>;
   };

   void process(const Message auto& m);   // generic path
   void process(const ___ auto& m);       // fast path: the compiler picks
                                          // the more ___ overload
   ```
   Answer format: one line; three words in blank order, separated by commas.

<details><summary>Answer</summary>
Key:
  - `Message, MarketDataMessage, constrained`. Writing `Message<T>` inside the
    new concept is what lets the compiler prove that it subsumes `Message`.
</details>

### Card M2-Q7 · Apply
Q: All your feed messages have `length()`; market-data messages also have
   `seq()`. The C++98 code dispatches with a virtual `process()`. For messages
   whose type is known at compile time:
   (a) write the two concept definitions (one line each is fine);
   (b) say what the second concept must do so that `process(trade)` is not ambiguous.
   Answer format: (a) two lines of code; (b) one sentence.

<details><summary>Answer</summary>
Key:
  - (a) `concept Message = requires(const T& m) { { m.length() } -> std::convertible_to<std::size_t>; };`
    and `concept MarketDataMessage = Message<T> && requires(const T& m) { { m.seq() } -> std::convertible_to<std::uint64_t>; };`
    (each preceded by `template <typename T>`).
  - (b) It must be built from `Message<T> &&`, so its constraints include the same
    atomic constraint and subsume `Message`; restating `length()` makes the call ambiguous.
</details>

### Card M2-G5 · Spot the trap
Q: `func(std::int64_t(0))` is a build error: "call to 'func' is ambiguous".
   ```cpp
   template<typename T> concept A = sizeof(T) > 4;
   template<typename T> concept B = sizeof(T) > 4 && std::is_integral_v<T>;
   template<A T> void func(T) {}
   template<B T> void func(T) {}
   ```
   Why does `B` not subsume `A`, and what one change fixes it?
   Answer format: (a) one sentence why; (b) the corrected line for `B`.

<details><summary>Answer</summary>
Key:
  - (a) The two `sizeof(T) > 4` are separate atomic constraints written in
    different places, so the compiler cannot prove `B` implies `A` (clang: `constraint
    expressions cannot be considered equivalent unless they originate from the same concept`).
  - (b) `template<typename T> concept B = A<T> && std::is_integral_v<T>;` Then the
    call picks the `B` overload; `func(8.0)` still picks `A`.
</details>

## Remember

a concept names compile-time Boolean requirements. This `Addable` checks
syntax alone; other concepts can check more. Build narrow concepts from wide
ones, or the compiler cannot rank
them. Tomorrow the misses come back first. Next: type traits, `if constexpr`
and fold expressions.

Further reading: [the 2009 committee discussion of concepts](https://open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2920.html).
