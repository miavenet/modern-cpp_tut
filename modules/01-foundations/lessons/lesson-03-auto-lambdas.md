# Lesson 03 — `auto` and lambdas: let the compiler name the type

A one-character slip, `auto` instead of `auto&`, can make a loop update copies
and leave your order book unchanged, with no warning. A lambda that captures a
reference can run after that reference has died. By the end of this lesson you
can predict what `auto` deduces, choose between `auto`, `auto&` and
`const auto&`, replace a C++98 functor with a lambda, and choose captures that
are still valid when a callback runs later.

**Time:** about 45 minutes across three sittings of at most 20 minutes.
Suggested stops: after the `auto` cards, then after capture lifetimes.
**You need:** lessons 01 and 02 (`std::move`, moved-from objects, `unique_ptr`).

## Pretests — guess first

### Card M1-P3 · Pretest
Q: Guess. In C++98 you write
   `std::map<std::string, std::vector<Order> >::iterator it = orders.begin();`.
   C++11 lets you write `auto it = orders.begin();`. Is `it` then dynamically
   typed, like a JavaScript `var`? What type does it have? One sentence.

<details><summary>Answer</summary>
Key:
  - No. `auto` asks the compiler to work out the type from the initializer, once,
    at compile time. `it` has exactly the type you would have written,
    `std::map<std::string, std::vector<Order>>::iterator` (assuming `orders` is
    not const), and the program means the same as if you had written it out.
</details>

### Card M1-P4 · Pretest
Q: Guess. In C++98, sorting orders by price needs a comparator class:
   ```cpp
   // C++98 fragment: Order has a member 'double price'; needs <algorithm>.
   struct PriceCompare {
       bool operator()(const Order& a, const Order& b) const {
           return a.price < b.price;
       }
   };
   std::sort(orders.begin(), orders.end(), PriceCompare());
   ```
   C++11 adds *lambda expressions*. What might one look like here, and what
   problem does it solve? One or two sentences.

<details><summary>Answer</summary>
Key:
  - A lambda is an unnamed function object written where it is used:
    `std::sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) { return a.price < b.price; });`
  - It removes the separate class. It can also *capture* local variables, which
    a C++98 functor needed constructor arguments and members for.
</details>

---

## What `auto` deduces

### CONCEPT C7  `auto`: declare a new object or request a reference

Long C++98 iterator types made a one-line loop declaration harder to read
than the loop. Type deduction removes the spelling, but the choice to copy
or refer to an object still belongs to you.

**The C++98 way.** You spell every type, for example
`std::map<std::string, std::vector<Order> >::const_iterator`. (C++98 also
needs the space in `> >`; C++11 accepts `>>`.)

**The modern way (C++11).** In `auto x = expr;`, the compiler deduces `x`'s
fixed type from the initializer.

**The rules you need.** Plain `auto` makes a new object, so it drops a
reference and a top-level `const` (const on the variable itself), just like
copying into a new variable. `auto&` makes a reference and keeps the `const` of
what it refers to. `const auto&` is a read-only reference: no copy.

A string literal is an array of `const char`, so `auto` turns it into
`const char*`, not `std::string`. `decltype(x)` below means "the declared type of `x`";
`static_assert` stops the build if a condition is false, so this program
compiling is the proof:

```cpp
#include <type_traits>
#include <vector>

int main() {
    auto a = 42;          static_assert(std::is_same_v<decltype(a), int>);
    auto b = 42.0;        static_assert(std::is_same_v<decltype(b), double>);
    auto c = "hello";     static_assert(std::is_same_v<decltype(c), const char*>);
    const int& r = a;
    auto d = r;           static_assert(std::is_same_v<decltype(d), int>);         // const and & dropped
    auto& e = r;          static_assert(std::is_same_v<decltype(e), const int&>);  // & keeps const
    std::vector<int> v{1, 2};
    const auto& f = v;    static_assert(std::is_same_v<decltype(f), const std::vector<int>&>);
    (void)b; (void)c; (void)d; (void)e; (void)f;
}
```

It compiles cleanly with `-std=c++20 -Wall -Wextra` (the `_v` helpers are
C++17).

**The trap.** "`auto` keeps whatever the initializer was" is wrong: from a
`const int&`, plain `auto` gives a fresh, modifiable `int` copy.

**Rule:** plain `auto` declares a new object of the deduced type; `auto&`
borrows and preserves constness. Fine print: an ordinary lvalue is copied,
other initializers can move or construct directly, and proxy types need care.


### Card M1-C3 · Fix it
Q: Name the type each variable gets.
   ```cpp
   // Fragment: statements inside a function; needs <vector>.
   std::vector<int> vec{1, 2, 3};
   auto a = 42;
   auto b = 42.0;
   auto c = "hello";
   const auto& d = vec;
   ```
   Answer format: one line; the four types for a, b, c, d in order, separated
   by commas.

<details><summary>Answer</summary>
Key:
  - a: `int`
  - b: `double`
  - c: `const char*` (a string literal decays to a pointer; not `std::string`)
  - d: `const std::vector<int>&`
  - Typed answer: `int, double, const char*, const std::vector<int>&`
</details>

### Card M1-Q5 · Recall
Q: Two parts, one sentence each.
   (a) `const int& x = limit; auto y = x;` What type is `y`, and why?
   (b) When do you write `auto&`, and when `const auto&`, instead of `auto`?
   Answer format: one line; separate labeled parts with semicolons.
Hint: Plain `auto` always declares a new object.


<details><summary>Answer</summary>
Key:
  - (a) `int`: plain `auto` declares a new object, so the reference and the
    top-level `const` are dropped and `y` is an independent copy.
  - (b) `auto&` when you want to modify the original without copying;
    `const auto&` when you only read it and want to avoid a copy (for example
    each element of a large container).
</details>

---

## Loops that copy, and a declaration that is not an object

### CONCEPT C8  `auto` in a range-for loop, and brace initialisation

A shorter loop can hide a copy on every iteration. If the body changes the
loop variable, that copy can even turn a plausible update into a no-op.

**Range-for (C++11).** `for (auto e : v)` declares `e` once per element, so the
`auto` rules from C7 apply: plain `auto` copies ordinary elements. Changes to
`e` change the copy. Fine print: `vector<bool>` yields a proxy whose copied
form can still modify the original bit. For a `std::map<K, T>`, each element is a
`std::pair<const K, T>`, so a copy duplicates both key and value.

```cpp
#include <cstdio>
#include <vector>

struct BookEntry {
    double price = 0;
    BookEntry() = default;
    BookEntry(const BookEntry& o) : price(o.price) { std::puts("  copy"); }
};

int main() {
    std::vector<BookEntry> entries(2);
    for (auto e : entries) e.price = 1.5;    // copies each element
    std::printf("after auto:  %.1f\n", entries[0].price);
    for (auto& e : entries) e.price = 1.5;   // refers to each element
    std::printf("after auto&: %.1f\n", entries[0].price);
}
```

**Predict first:** how many `copy` lines, and the two prices?

<details><summary>Output (Apple clang 21)</summary>

```
  copy
  copy
after auto:  0.0
after auto&: 1.5
```
</details>

If the container is `const`, `auto&` deduces `const BookEntry&`, and calling a
non-const member through it is a compile error. One more form: `auto&&` binds
to anything the loop produces, including temporary *proxy* objects such as the
ones `std::vector<bool>` returns, which `auto&` cannot bind to.

**Brace initialisation (C++11).** C++98 has a parsing rule: anything that can be
read as a declaration is one. So `Order order();` declares a *function* named
`order` that returns an `Order`. Clang warns: `empty parentheses interpreted as
a function declaration [-Wvexing-parse]`. This is the "most vexing parse".

Braces cannot declare a function: `Order order{};` declares an object using
list-initialization. For an aggregate, this performs aggregate initialization;
otherwise the applicable constructor rules decide what happens.

**Rule:** use `auto&` or `const auto&` in loops over things you do not want to
copy, and use `{}` when you mean "construct an object with no arguments".


### Card M1-Q6 · Apply
Q: Two parts, one sentence each.
   ```cpp
   // Fragment: getEntries() returns const std::vector<BookEntry>&;
   // BookEntry, orderBook and now() are your own code.
   for (auto entry : orderBook.getEntries()) {
       entry.updateTimestamp(now());          // a non-const member function
   }
   ```
   (a) What does this loop actually update?
   (b) A colleague changes `auto` to `auto&`. What happens, and what does that
   tell you?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Only copies: `auto entry` copies each `BookEntry`, the copy gets the new
    timestamp and is destroyed; the book is unchanged (and every entry was
    copied for nothing).
  - (b) It no longer compiles: `auto&` deduces `const BookEntry&`, and a
    non-const member cannot be called through it. The function returns read-only
    access, so either the loop should only read (`const auto&`) or the class
    needs a non-const accessor; the copy was hiding that design question.
</details>
Stretch: What does `for (auto&& entry : ...)` mean, and when is it useful?
<details><summary>Optional extension answer</summary>
StretchKey:
  - It binds to whatever the loop yields, lvalue or temporary, without copying;
    it is needed for proxy elements such as those of `std::vector<bool>`, which
    `auto&` cannot bind to.
</details>

### Card M1-G7 · Spot the trap
Q: Two parts.
   ```cpp
   // Fragment: OrderBook has a non-const updatePrice(double); needs <map>, <string>.
   std::map<std::string, OrderBook> books;   // N entries
   for (auto entry : books) {
       entry.second.updatePrice(newPrice);
   }
   ```
   (a) What is copied, and how many times? One sentence.
   (b) Write the corrected loop header. One line.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `entry` is a `std::pair<const std::string, OrderBook>` copied from each
    element: N copies of a key string and a whole `OrderBook`. The updates go to
    those copies and are lost.
  - (b) `for (auto& entry : books)`, or with C++17 structured bindings
    `for (auto& [symbol, book] : books)`, then `book.updatePrice(newPrice);`.
</details>

### Card M1-G9 · Spot the trap
Q: Two parts, one line each. `Order order();`
   (a) Does this create an object or declare a function?
   (b) Write a version that creates an object rather than declaring a function.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) It declares a function named `order` taking no arguments and returning
    `Order` (the "most vexing parse"; clang warns `-Wvexing-parse`).
  - (b) `Order order{};` (C++11).
</details>

---

## Lambdas

### CONCEPT C9  A lambda is an unnamed function object with captures

**A whole class for "less than 17".** In 2006, Jeremiah Willcock, Jaakko
Järvi, Doug Gregor, Bjarne Stroustrup and Andrew Lumsdaine showed the price of
one tiny `find_if` test. C++98 needed a named struct, `less_than_i`, with a
member, a constructor and an `operator()`, defined outside the function using it.

Their verdict: it was "so verbose that it would be much easier (and likely
clearer to readers of the code) to write a for loop". Library tricks like
Boost.Lambda's `_1 < i` were terse, but users hit walls; one support query asked
why `_1.size()` would not compile.

So they had the compiler write the class: a closure, an anonymous function object
that stores the local variables it uses. That is the memory hook: the lambda is
an object you made now, which may be called much later.

**The C++98 way.** Write a class with `operator()`, plus a member and a
constructor for every local value it needs (see the pretest's `PriceCompare`).

**The modern way (C++11).** A lambda expression,
`[captures](parameters) { body }`, makes the compiler write that class for you
and create one object of it, called the *closure*. Each lambda has its own
unnamed type, which is why you store one in `auto`. The *capture list* says
which local variables the body may use:

```
[x]    copy of x, taken when the lambda is created
[&x]   reference to x: the lambda sees x's later changes, and needs x alive
[=]    copy local variables that need capturing
[&]    refer to local variables that need capturing
[]     capture nothing
```

```cpp
#include <algorithm>
#include <cstdio>
#include <vector>

struct Order { int qty; double notional; };

int main() {
    std::vector<Order> orders{{100, 5e5}, {300, 2e6}, {50, 1e4}};
    double threshold = 1e5;
    auto risky = std::count_if(orders.begin(), orders.end(),
        [threshold](const Order& o) { return o.notional > threshold; });
    threshold = 0;   // too late: the lambda already copied 1e5
    if (!orders.empty()) {   // max_element on an empty range returns end()
        auto biggest = std::max_element(orders.begin(), orders.end(),
            [](const Order& a, const Order& b) { return a.qty < b.qty; });
        std::printf("risky=%ld biggest qty=%d\n", (long)risky, biggest->qty);
    }
}
```

**Predict first:** what are `risky` and the biggest quantity?

<details><summary>Output (Apple clang 21)</summary>

```
risky=2 biggest qty=300
```

Two orders have notional above 1e5. The second lambda uses no outside
variable, so it needs no capture at all.
</details>

**The trap.** "`[&]` captures every variable in scope" is wrong: a default
capture only captures what the body actually uses.

**Rule:** a lambda is a functor the compiler writes for you; its capture list
decides whether it holds copies or references.


### Card M1-Q8 · Apply
Q: Rewrite this C++98 code with a lambda, keeping its behaviour. Answer: the
   new statement, on one line.
   ```cpp
   // C++98 fragment: Order has an int member qty; getLotSize is your own code.
   struct SizeFilter {
       int minSize_;
       SizeFilter(int minSize) : minSize_(minSize) {}
       bool operator()(const Order& o) const { return o.qty >= minSize_; }
   };
   int minLot = getLotSize(symbol);
   orders.erase(std::remove_if(orders.begin(), orders.end(), SizeFilter(minLot)),
                orders.end());
   ```

   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - `orders.erase(std::remove_if(orders.begin(), orders.end(), [minLot](const Order& o) { return o.qty >= minLot; }), orders.end());`
  - Same predicate (`>=`), so it still removes orders whose quantity is at
    least `minLot`. If the business rule was to remove the small ones, changing
    the comparison is a separate bug fix.
  - `minLot` is captured by copy: it is a cheap `int` and the lambda is used
    immediately.
</details>
Stretch: How does C++20's `std::erase_if` shorten this?
<details><summary>Optional extension answer</summary>
StretchKey:
  - `std::erase_if(orders, [minLot](const Order& o) { return o.qty >= minLot; });`
    one call replaces the erase-remove pair (C++20, `__cpp_lib_erase_if`).
</details>

---

## Lambdas that run later

### CONCEPT C10  A stored lambda must not outlive what it refers to

**The danger.** A lambda stored in a callback, a queue or another thread may run
after the function that created it has returned. Every reference it captured
must still refer to a living object then. If not, the reference *dangles*, and
using it is undefined behaviour.

**A reference parameter is subtle.** In
`std::function<void()> makeCallback(const std::string& symbol)`, returning
`[&symbol] { log(symbol); }` refers to the *caller's* string. Returning does
not destroy that string, but the caller may: `makeCallback("AAPL")` binds
`symbol` to a temporary string that dies at the end of that statement.

Capture a copy (`[symbol]`, or C++14's *init-capture* `[s = symbol]`) when you cannot
guarantee the lifetime.

**Moving into a capture.** For `std::string`, moving into a capture requires
a non-const source; `std::move` on a const string selects copying:

```cpp
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

std::function<void()> fromConstRef(const std::string& symbol) {
    return [s = std::move(symbol)] { std::printf("  lambda has %s\n", s.c_str()); };
}
std::function<void()> fromValue(std::string symbol) {
    return [s = std::move(symbol)] { std::printf("  lambda has %s\n", s.c_str()); };
}

int main() {
    std::string a = "AAPL-long-symbol-to-avoid-sso";
    auto f = fromConstRef(a);
    std::printf("caller's a after const& version: '%s'\n", a.c_str());
    f();
    std::string b = "MSFT-long-symbol-to-avoid-sso";
    auto g = fromValue(std::move(b));
    std::printf("caller's b after by-value version: '%s'\n", b.c_str());
    g();
}
```

**Predict first:** after each call, is the caller's string still there?

<details><summary>Output (Apple clang 21, libc++)</summary>

```
caller's a after const& version: 'AAPL-long-symbol-to-avoid-sso'
  lambda has AAPL-long-symbol-to-avoid-sso
caller's b after by-value version: ''
  lambda has MSFT-long-symbol-to-avoid-sso
```

`a` is untouched: it was copied. `b` was moved through a by-value parameter
(its emptiness is what libc++ shows; in general it is unspecified).
</details>

**Members mean `this`.** Inside a member function, `[=]` does not copy data
members. A member `logPath_` is really `this->logPath_`, so `[=]` captures the
`this` pointer, and the lambda dangles once the object is destroyed.

C++20 deprecates this implicit capture; clang warns `implicit capture of 'this' with
a capture default of '=' is deprecated [-Wdeprecated-this-capture]`. Safe
choices: `[path = logPath_]` (C++14, copies one member), `[*this]` (C++17,
copies the whole object), or `[this]` when you know the object outlives the
lambda.

**Rule:** a delayed callback may borrow only when its referents outlive every
invocation. Fine print: copying a raw pointer or view does not extend the
pointed-to object's lifetime.


| Capture | Holds | Good when | Smell |
|---|---|---|---|
| `[x]` | A copy made now | Callback may outlive the local | You expected later changes to `x` |
| `[&x]` | A reference | Call finishes while `x` lives | Callback is queued or returned |
| `[x = std::move(x)]` | An owned moved value | Move-only or costly state travels with callback | Code reads the old `x` afterward |
| `[this]` | An object pointer | Owner definitely outlives callback | Callback is stored without a lifetime rule |

Advice: choose captures by the callback's lifetime, and treat a queued
reference capture as a review alarm.

### Card M1-J3 · Choose and explain
Q: A review bot flags every reference capture as "may dangle". This one
   passes a lambda to the order book's own `forEachLevel`, shown in full. Is
   the AI-written `[&total]` right here? What would you check first, and what
   would change your answer? Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   #include <vector>
   struct Book {
       std::vector<int> levels{100, 250, 50};
       template <class F> void forEachLevel(F&& f) const {
           for (int q : levels) f(q);
       }
   };
   int sumDepth(const Book& b) {
       int total = 0;
       b.forEachLevel([&total](int q) { total += q; });
       return total;
   }
   int main() { std::printf("%d\n", sumDepth(Book{})); }
   ```

<details><summary>Answer</summary>
Key:
  - It is right here: `forEachLevel` calls the lambda only before it returns and keeps nothing, so `total` outlives every call and the program prints `400` (a copy `[total]` would not even compile, since the body assigns to it).
  - First check what the callee does with the callable: invoke it during the call, or store it in a member, queue or `std::function` for later.
  - If `forEachLevel` saved the lambda to run on a later tick, `&total` would dangle after `sumDepth` returns and I would reject it.
</details>

### Card M1-C4 · Fix it
Q: This lambda runs later on another thread. Fill the capture list so it is
   safe and the lambda owns the order.
   ```cpp
   // Fragment: Order, RISK_LIMIT, reject and threadPool are your own code.
   void scheduleRiskCheck(Order order) {
       threadPool.submit([___]() {
           if (order.notional > RISK_LIMIT) reject(order);
       });
   }
   ```
   Answer format: one line; only what goes between the brackets.

<details><summary>Answer</summary>
Key:
  - `order` (C++11 copy capture) or `order = std::move(order)` (C++14
    init-capture; moves, because the parameter is a by-value, non-const copy).
    The body still says `order`, so the captured name must be `order`.
  - `&order` is wrong: it refers to the parameter, which is destroyed when
    `scheduleRiskCheck` returns, before the lambda runs.
</details>

### Card M1-Q7 · Recall
Q: A stored callback captures a local `x` by reference. Another captures a raw
   pointer to `x` by value. Both run after the function returns. Is either
   safe, and what would make it safe? Answer format: two short sentences.


<details><summary>Answer</summary>
Key:
  - Neither is safe after `x` dies. Copy the needed value, retain ownership, or
    keep `x` alive until every call finishes; copying its address is insufficient.
</details>

### Card M1-G8 · Spot the trap
Q: Two parts, one sentence each.
   ```cpp
   // Fragment: writeToFile(const std::string&) is your own code.
   class TradeLogger {
       std::string logPath_;
   public:
       auto getWriter() { return [=]() { writeToFile(logPath_); }; }
   };
   ```
   The writer is called after its `TradeLogger` was destroyed, and crashes.
   (a) What did `[=]` actually capture? (b) Give a capture list that fixes it.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) The `this` pointer, not a copy of `logPath_`; the body reads
    `this->logPath_` through a dangling pointer. (This implicit capture is
    deprecated in C++20; clang warns `-Wdeprecated-this-capture`.)
  - (b) `[path = logPath_]` (C++14; then call `writeToFile(path)`), or
    `[*this]` (C++17; copies the whole logger).
</details>

---

## Passing lambdas to functions

### CONCEPT C11  `std::function` versus a template parameter

The same lambda may be called once in a tight loop or stored among unlike
callbacks. Those jobs need different interfaces, because one needs the
callable's exact type and the other needs one common type.

**Two ways to accept "any callable".** A template,
`template <typename F> void filterOrders(F pred)`, is compiled separately for
each lambda type, so the compiler sees the exact lambda and can inline its body.
`std::function<bool(const Order&)>` (C++11) is one fixed type that can hold
compatible copy-constructible callables.

It does this by *type erasure*: it stores the
callable inside itself, or possibly on the heap. Whether it fits inline depends
on the library and properties such as size, alignment, and copy behavior.
Type erasure can add indirect dispatch, although optimization may remove it
when the target is known.

```cpp
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>

static int allocations = 0;   // count calls to this ordinary scalar allocation overload
void* operator new(std::size_t n) { ++allocations; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <typename F> int countIf(F pred) { int n = 0; for (int i = 0; i < 10; ++i) n += pred(i); return n; }
int countIfErased(const std::function<bool(int)>& pred) { int n = 0; for (int i = 0; i < 10; ++i) n += pred(i); return n; }

int main() {
    int limit = 5;
    std::array<int, 16> table{};                       // 64 bytes captured by copy
    auto small = [limit](int x) { return x < limit; };
    auto big = [limit, table](int x) { return x < limit + table[0]; };
    allocations = 0; int a = countIf(small);       std::printf("template, small: %d, allocations %d\n", a, allocations);
    allocations = 0; int b = countIfErased(small); std::printf("std::function, small: %d, allocations %d\n", b, allocations);
    allocations = 0; int c = countIfErased(big);   std::printf("std::function, big: %d, allocations %d\n", c, allocations);
}
```

<details><summary>Output (Apple clang 21, libc++, -O0 and -O2)</summary>

```
template, small: 5, allocations 0
std::function, small: 5, allocations 0
std::function, big: 5, allocations 1
```

Whether a callable can be stored inline depends on the library and properties
such as size, alignment, and copy behavior.
</details>

**The trap.** Two wrong beliefs: "`std::function` always allocates" (the small
lambda did not) and "it is free" (the big one allocated). Type erasure can add
indirect dispatch, although optimization may remove it when the target is
known. How much this matters depends on your code: measure it.

**Rule:** take a template parameter on hot paths; use `std::function` when you
must store callables of different types in one place.


### Card M1-G10 · Spot the trap
Q: `void filterOrders(std::function<bool(const Order&)> pred);` is called with
   a lambda for every message. A colleague suggests
   `template <typename F> void filterOrders(F pred);`. Two parts, one sentence
   each: (a) what work can `std::function` add that the template avoids?
   (b) when is `std::function` the right choice anyway?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Type erasure may add allocation and indirect dispatch. Inline storage
    depends on library properties such as size, alignment, and copy behavior;
    optimization may remove the dispatch when the target is known. The
    template sees the lambda's exact type. Measure before quoting a number.
  - (b) When callables of different types must be stored together or chosen at
    run time, for example a table of registered callbacks.
</details>

---

## Remember

- Plain `auto` declares a new object; `auto&` borrows and preserves constness.
- A lambda is a functor the compiler writes; its captures are copies or
  references.
- A delayed callback may borrow only when its referents outlive every call.

Missed cards come back tomorrow. Next: lesson 04, five everyday C++11 tools.
