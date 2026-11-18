# Lesson 04 — Everyday C++11: enums, time, arrays, braces, per-thread counters

`setTimeout(100);` One hundred what: seconds, milliseconds, microseconds? With
a plain integer parameter, the unit lives in a name or comment; C++98
programmers could also define a wrapper type. This lesson covers five small C++11
features, each replacing a C++98 habit that let a bug compile. At the end you
can write a 1-byte scoped enum, time code with the right clock, choose
`std::array` over a C array, predict what braces construct, and count per
thread without a lock.

**Time:** about 35 minutes; two sittings is fine (stop after the clock cards).
**You need:** lesson 03 (`auto`, brace initialisation).

## Pretests — guess first

### Card M1-P5 · Pretest
Q: Guess. In C++98, `enum Side { BUY, SELL };` and `enum Action { BUY, CANCEL };`
   sit in the same scope. What happens? How might C++11 fix it? One sentence.

<details><summary>Answer</summary>
Key:
  - C++98: it does not compile. Plain enum names land in the surrounding
    scope, so `BUY` is declared twice (clang: `redefinition of enumerator
    'BUY'`). Plain enums also convert to `int` silently.
  - C++11 adds `enum class`: its names stay inside it (`Side::Buy`) and it does
    not convert to `int` implicitly.
</details>

### Card M1-P6 · Pretest
Q: Guess. To time an order-handling function, C++98 code often calls
   `gettimeofday()`. What does C++11 offer instead, and which clock would you
   pick for measuring an elapsed time? One sentence.

<details><summary>Answer</summary>
Key:
  - The `<chrono>` library (C++11): typed durations and clocks.
  - `std::chrono::steady_clock`: it never goes backwards, unlike
    `system_clock`, which follows the wall clock and can be adjusted (for
    example by a time-sync daemon).
</details>

---

## Scoped enums

### CONCEPT C12  `enum class`: scoped names, no silent `int`, chosen size

**`armWeapons`: "ok; oops".** In 2004 Herb Sutter and David Miller opened
their strongly typed enum proposal by quoting Stroustrup: C enumerations are "a
curiously half-baked concept".

Their first example compared an alert level with a colour:
`bool armWeapons = ( a >= ClrYellow );`. It compiles, because both enums turn
silently into integers. Their comment on that line: "ok; oops".

Next came a network `Packet` whose enum `Version` field was marked "bad, size can
vary by implementation". As they put it, "Today's workarounds boil down to not
using enums".

They named three faults: implicit integer conversion, an underlying type you
cannot choose, and names leaking into the surrounding scope. `enum class` fixes
all three, so when you see an enum in a protocol, ask whether its values can mix
with another domain and whether its size is actually specified.

**The C++98 way.** `enum Side { BUY, SELL };` puts `BUY` and `SELL` into the
surrounding scope, converts silently to `int`, and its size is up to the
compiler.

**The modern way (C++11).** `enum class Side : std::uint8_t { Buy, Sell };`.
The names are reached as `Side::Buy`. There is no implicit conversion to `int`,
so `int x = Side::Buy;` is an error (`cannot initialize a variable of type
'int' with an rvalue of type 'Side'`). After the colon you choose the
*underlying type*, the integer type that stores it; `std::uint8_t` makes it 1
byte. `enum struct` means exactly the same as `enum class`.

```cpp
#include <cstdint>
#include <cstdio>
#include <type_traits>

enum class Side : std::uint8_t { Buy, Sell };
enum class Action : std::uint8_t { Buy, Cancel };   // fine: names are scoped
enum struct OrderType : std::uint8_t { Market, Limit, Stop };
static_assert(sizeof(Side) == 1);
static_assert(sizeof(OrderType) == 1);

int main() {
    Side s = Side::Sell;
    auto raw = static_cast<std::uint8_t>(s);                           // explicit: fine
    auto same = static_cast<std::underlying_type_t<Side>>(s);          // C++14 alias
    std::printf("Sell is %d, %d\n", raw, same);
}
```

<details><summary>Output (Apple clang 21)</summary>

```
Sell is 1, 1
```
</details>

**The trap.** No implicit `int` also means no built-in arithmetic or bitwise
operators. For flags, `Flags::Read | Flags::Write` fails with `invalid operands
to binary expression ('Flags' and 'Flags')`. You write the operator yourself:

```cpp
// Fragment: needs <cstdint>.
enum class Flags : std::uint8_t { Read = 1, Write = 2 };
constexpr Flags operator|(Flags a, Flags b) {
    return static_cast<Flags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
```

**Rule:** use `enum class` with an explicit underlying type; convert to an
integer only with a visible `static_cast`.


| Choice | Guarantee | Cost or obligation | Right when |
|---|---|---|---|
| Plain `enum` | Enumerators enter surrounding scope; converts to integer | Accidental cross-domain arithmetic can compile | Legacy API requires it |
| `enum class` | Scoped names; no implicit integer conversion | Cast explicitly at integer boundary | Values represent a distinct domain |

Advice: use `enum class` for a new domain; a plain enum leaking into a wire
format or arithmetic expression is a smell.

### Card M1-C5 · Fix it
Q: Fill the two blanks so `OrderType` is a scoped enum that occupies exactly 1
   byte.
   ```cpp
// Fragment: fill the blanks in this exercise.
   enum ___ OrderType : ___ { Market, Limit, Stop };
   ```
   Answer format: one line; the two blanks in order, separated by a comma.

<details><summary>Answer</summary>
Key:
  - Blank 1: `class` (or `struct`, which means the same).
  - Blank 2: a 1-byte integer type: `std::uint8_t` (also accepted: `int8_t`,
    `char`, `unsigned char`, `signed char`). The check compiles your answer with
    `static_assert(sizeof(OrderType) == 1)`.
  - Typed answer: `class, uint8_t`
</details>

### Card M1-G13 · Spot the trap
Q: Two parts, one sentence each.
   `enum class Flags : std::uint8_t { Read = 1, Write = 2 };`
   `Flags f = Flags::Read | Flags::Write;`
   (a) What does the compiler do with the second line, and why?
   (b) How do you make `|` work?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) It refuses to compile it (`invalid operands to binary expression`):
    an `enum class` does not convert to an integer, so the built-in `|` does
    not apply.
  - (b) Define `operator|` for `Flags`, casting both operands to the underlying
    type, combining them, and casting back.
</details>

---

## Time with types

### CONCEPT C13  `<chrono>`: durations with units, and the clock that never jumps

**The C++98 way.** `void setTimeout(int timeoutMs);` The unit lives in a name or
a comment, and `setTimeout(5)` meaning five seconds compiles fine.

**The modern way.** A *duration* (C++11) is a number plus a unit in its type:
`std::chrono::milliseconds`, `seconds`, `microseconds`. For integral
representations, conversion to finer units can be implicit when the conversion
ratio is integral; conversion to coarser units needs an explicit cast.
Conversion to an integral representation truncates fractional ticks toward
zero. Fine print: floating representations follow different conversion rules.
C++14 adds literals: after `using namespace
std::chrono_literals;` you can write `100ms`, `2s`, `1500us`.

```cpp
#include <chrono>
#include <cstdio>
using namespace std::chrono_literals;   // C++14

void setTimeout(std::chrono::milliseconds t) { std::printf("timeout %lld ms\n", (long long)t.count()); }

int main() {
    setTimeout(100ms);
    setTimeout(2s);              // seconds -> milliseconds: exact, allowed
    // setTimeout(1500us);       // error: would truncate
    // setTimeout(100);          // error: a bare int has no unit
    setTimeout(std::chrono::duration_cast<std::chrono::milliseconds>(1500us));
}
```

**Predict first:** the three printed timeouts.

<details><summary>Output (Apple clang 21)</summary>

```
timeout 100 ms
timeout 2000 ms
timeout 1 ms
```
</details>

**Clocks.** `steady_clock::now()` gives a time point that never goes backwards,
so `end - start` measures an interval on that clock. `system_clock` follows
the wall clock, which can be adjusted. `high_resolution_clock` may be an alias
for either; on this toolchain (libc++) it is `steady_clock`.

**The trap.** "Durations make unit bugs impossible" is too strong:
`duration_cast` truncates, very large counts can overflow, and once you call
`.count()` you hold a plain number again.

**Rule:** pass durations, not integers, and time intervals with
`steady_clock`.


| Clock | What it represents | Cost or obligation | Right when |
|---|---|---|---|
| `steady_clock` | Monotonic time points | Does not represent calendar time | Measuring elapsed intervals |
| `system_clock` | Wall-clock time points | Time adjustments can change intervals | Timestamps for people or logs |
| `high_resolution_clock` | Implementation-chosen clock | Check whether it is steady here | Only after checking its actual alias |

Advice: time a duration with `steady_clock`; a negative elapsed measurement
is a smell that a wall clock was used.

### Card M1-J4 · Choose and explain
Q: The compliance log must record when each order arrived, so the stamps can
   be matched against the exchange's own timestamps. An AI assistant wrote
   this with `system_clock`, and a reviewer says "always use `steady_clock`".
   Is the clock right here? What would you check first, and what would change
   your answer? Answer format: three short sentences.
   ```cpp
   #include <chrono>
   #include <cstdio>
   long long arrivalStampMs() {
       using namespace std::chrono;
       return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
   }
   int main() {
       long long t = arrivalStampMs();
       std::printf("arrived at %lld ms since epoch\n", t);
   }
   ```

<details><summary>Answer</summary>
Key:
  - `system_clock` is right here, because the log needs wall-clock time that another firm's clock can be compared with, while `steady_clock` is not related to wall-clock time (it can be time since the last reboot).
  - Use `system_clock` for a wall-clock stamp; check epoch, units, resolution,
    and clock synchronization between firms.
  - Use `steady_clock` for elapsed intervals within one clock domain. Neither
    clock choice alone guarantees accurate cross-machine latency.
</details>

### Card M1-C6 · Fix it
Q: Fill the blank (the same clock name both times) so this measures elapsed
   time correctly even if someone adjusts the system time meanwhile.
   ```cpp
   // Fragment: inside a function; needs <chrono>.
   auto start = std::chrono::___::now();
   // ... do work ...
   auto end = std::chrono::___::now();
   ```
   Answer format: one word, the clock name.

<details><summary>Answer</summary>
Key:
  - `steady_clock`: its time never decreases, so `end - start` cannot be
    distorted by a clock adjustment.
</details>

### Card M1-G12 · Spot the trap
Q: A latency tool uses `std::chrono::high_resolution_clock`. On one machine
   it occasionally reports negative durations. In one or two sentences: how
   can that happen, and what should it use?

   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - `high_resolution_clock` may be an alias of `system_clock`, and the
    system clock can be set backwards by a time adjustment, so
    `end - start` can be negative.
  - Use `std::chrono::steady_clock` for elapsed time.
</details>

---

## Fixed-size arrays

### CONCEPT C14  `std::array`: a C array that keeps its size and copies

A C array's size can disappear at a function boundary. That makes a fixed
ten-element buffer look like an unchecked pointer inside the callee.

**The C++98 way.** `int levels[10];` In most expressions a C array turns
(*decays*) into a pointer to its first element. A function parameter written
`int levels[10]` is really `int* levels`, so the size is lost; you cannot assign
one array to another or return one by value.

**The modern way (C++11).** `std::array<int, 10>` holds its ten elements inline,
just like the C array (no heap allocation of its own), but it is a class: it
keeps its size (`.size()`), can be copied, assigned and returned by value, has
iterators for the standard algorithms, and `at(i)` checks the index (throws
`std::out_of_range`; `operator[]` requires a valid index, so do not rely on
it to report an out-of-range error).

```cpp
#include <array>
#include <cstdio>

void takesCArray(int levels[10]) {           // really 'int* levels'
    std::printf("inside: sizeof(levels)=%zu\n", sizeof(levels));
}
void takesStdArray(const std::array<int, 10>& levels) {
    std::printf("inside: levels.size()=%zu\n", levels.size());
}
std::array<int, 3> topThree() { return {101, 100, 99}; }   // returned by value

int main() {
    int c[10] = {};
    std::array<int, 10> a{};
    std::printf("outside: sizeof(c)=%zu sizeof(a)=%zu\n", sizeof(c), sizeof(a));
    takesCArray(c);
    takesStdArray(a);
    std::array<int, 3> best = topThree();
    std::array<int, 3> copy = best;          // C arrays cannot be copied like this
    std::printf("copy[0]=%d\n", copy[0]);
}
```

**Predict first:** the four sizes printed.

<details><summary>Output (Apple clang 21, arm64)</summary>

```
outside: sizeof(c)=40 sizeof(a)=40
inside: sizeof(levels)=8
inside: levels.size()=10
copy[0]=101
```

Clang also warns: `sizeof on array function parameter will return size of
'int *' instead of 'int[10]'`. Same 40 bytes outside; only the C array lost its
size.
</details>

**The trap.** "`std::array` lives on the stack" is a shortcut: its elements are
inside the object, wherever the object lives (a local, a global, or inside a
heap-allocated struct).

**Rule:** use `std::array` when the size is fixed at compile time; it keeps
its size and value semantics without an allocation of its own. Fine print:
exact object size is implementation-dependent, and `std::array<T, 0>` still
occupies storage.


### Card M1-Q9 · Recall
Q: Two parts, one sentence each. Give two specific advantages of
   `std::array<int, 10>` over `int[10]` when passing and returning data:
   (a) for passing it to a function, (b) for returning or copying it.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) It does not decay to a pointer, so the callee still knows the size
    (`.size()`, and the size is part of the type).
  - (b) It can be returned by value, copied and assigned with `=`, which C
    arrays cannot. (Also acceptable: iterators for algorithms, or `at()` bounds
    checks.)
</details>

---

## What braces construct

### CONCEPT C15  Braces prefer the `initializer_list` constructor

Braces help prevent narrowing, but they also give containers a second
interpretation of constructor arguments. A two-number vector declaration
can therefore mean two values or a count and a fill value.

**The C++98 way.** One approach was repeated `push_back`; another was
constructing from an array's iterator range.

**The modern way (C++11).** `std::vector<int> v = {1, 2, 3};` A braced list of
values becomes a `std::initializer_list<int>`, and a class can have a
constructor taking one. Braces also refuse *narrowing*: `int ticks{101.5};` is
an error (`type 'double' cannot be narrowed to 'int' in initializer list`),
where `int ticks(101.5);` silently truncates.

**The surprise.** When a class has an `initializer_list` constructor, braces
strongly prefer it over the other constructors:

```cpp
#include <cstdio>
#include <vector>

int main() {
    std::vector<int> braces{5, 1};    // initializer_list: the elements 5 and 1
    std::vector<int> parens(5, 1);    // (count, value): five 1s
    std::printf("braces: size %zu; parens: size %zu\n", braces.size(), parens.size());
}
```

**Predict first:** the two sizes.

<details><summary>Output (Apple clang 21)</summary>

```
braces: size 2; parens: size 5
```
</details>

**Rule:** for `std::vector<int>`, `{5, 1}` supplies two elements; `(5, 1)`
supplies five copies of 1. Fine print: braces still select among applicable
constructors by overload resolution.


### Card M1-G11 · Spot the trap
Q: Two parts, one line each. `std::vector<int> v{5, 1};`
   (a) How many elements does `v` have, and what are they?
   (b) Write the declaration that gives five elements, all equal to 1.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Two elements, 5 and 1: braces pick the `initializer_list` constructor.
  - (b) `std::vector<int> v(5, 1);`
</details>

---

## One counter per thread

### CONCEPT C16  `thread_local`: each thread gets its own variable

A shared hot counter needs synchronization on every update. If each worker
only needs its own count until the end, separate storage avoids that shared
update inside the loop.

**One definition first.** In these examples, a *data race* occurs when
different threads make conflicting accesses to the same memory location, at
least one access is non-atomic, and neither access happens before the other.
The accesses need not physically overlap. A data race is undefined behavior.

**The C++98 way.** C++98 had no threads in the language. Per-thread storage
came from platform calls such as POSIX `pthread_key_create` or compiler
extensions such as `__thread`.

**The modern way (C++11).** `thread_local long processed = 0;` gives every
thread its own `processed`. A thread touching only its own copy cannot race
with others, so no lock and no atomic is needed. To get a total, each thread
adds its count to one shared atomic once, at the end.

```cpp
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

thread_local long processed = 0;          // one separate counter per thread
std::atomic<long> total{0};

void worker(int orders) {
    for (int i = 0; i < orders; ++i) ++processed;   // only this thread touches it
    total.fetch_add(processed);                     // one shared update per thread
}

int main() {
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back(worker, 1000000);
    for (auto& th : threads) th.join();
    std::printf("total=%ld main thread's own processed=%ld\n", total.load(), processed);
}
```

**Predict first:** `total`, and the main thread's own `processed`.

<details><summary>Output (Apple clang 21, arm64)</summary>

```
total=4000000 main thread's own processed=0
```

The main thread never incremented its copy, so it is still 0.
</details>

**The trap.** "`thread_local` is free" is not guaranteed. Reaching it can be a
plain memory access or a call into a runtime helper, depending on platform and
linking. On this toolchain, the optimized worker performs a TLS lookup through
a descriptor and an indirect call, then collapses the increment loop into one
addition. Do not infer a helper call per source-level increment. If it matters,
look at the generated code or measure.

**Rule:** keep hot counters per thread with `thread_local`, and combine them
once, through an atomic, when you need the total.


### Card M1-Q10 · Apply
Q: Four worker threads each count processed orders with
   `thread_local long processed;` and `++processed;`. Two parts, one sentence
   each: (a) why is there no data race without a lock or atomic? (b) how do you
   get the total across all four threads?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Each thread has its own `processed` variable, so no memory location is
    shared between threads.
  - (b) Each thread adds its count to one shared `std::atomic<long>` once (for
    example when it finishes), and the total is read after the threads are
    joined.
</details>

---

## Remember

- `enum class` with an explicit underlying type; cast visibly.
- Durations instead of integers; `steady_clock` for elapsed time.
- For this vector example, `{5,1}` supplies two elements, while `(5,1)`
  supplies a count and fill value. Braces still perform overload resolution.

Missed cards come back tomorrow. Next: lesson 05, the LPA lens on
`push_back` growth (then module 02, compile-time code).
