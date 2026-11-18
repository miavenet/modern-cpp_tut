# Lesson 01 — Move semantics: hand over the buffer instead of copying it

Your matcher builds a `std::vector<Fill>` with 10,000 fills and returns it. In
C++98 that return could copy all 10,000 fills. By the end of this lesson you can
tell which constructor runs (copy, move, or none) for a line of code, write a
move constructor, and avoid two classic mistakes: assuming a moved-from object
still has its old value and writing `return std::move(x);`.

**Time:** about 34 minutes (two short sittings is fine). **You need:** C++98
copy constructors and the primer below.

#### Course primer: what changed after C++98, and how this course works

C++ is revised by an ISO committee, and each version is named by its year.
C++03 was a small bug-fix release. C++11 was the big change; C++14, C++17, C++20
and C++23 followed. C++26 is the newest (its final draft dates from 2026), and
compilers support only parts of it. Every feature here is labelled with the
version that introduced it.

```
C++98/03     C++11              C++14          C++17           C++20          C++23
   |           |                  |              |               |              |
 copies,    move (&&),         make_unique,   if constexpr,   concepts,      std::expected
 raw new/   unique_ptr,        generic        fold            consteval
 delete     shared_ptr, auto,  lambdas        expressions
            lambdas, threads,
            atomics
```

The C++98 problem behind the first column of changes: the language could not say
"I am done with this object, take its insides". When elision or an explicit
transfer interface did not apply, passing a big object by value could copy it.
A raw pointer did not say who must `delete` it. Module 01 fixes that (move
semantics, smart pointers) and adds `auto` and lambdas. Module 02 is compile-time
code; 03 is threads and atomics; 04 the newer library types; 05 to 07 memory,
latency patterns and tooling.

**How a lesson works.** A *pretest* asks you to guess before you are taught;
try before looking, even when unsure. Then compare your answer with the
explanation and correct it. The attempt and feedback help it stick.
Retrieval may feel harder than rereading. Judge progress by what you recall
after a delay, not by today's feeling of familiarity. A
*concept* teaches one idea, and *cards* ask you to recall or apply it.

**Results.** Some cards are checked by a script, often by compiling your answer.
`HIT` means it passed. `MISS` means it failed; the line after it names the part
that was wrong, and you can `retry`. `SETUP PROBLEM` means the check itself could
not run (for example `clang++` is missing): run `./setup.sh`, then retry. Other
cards show the key and ask you to grade yourself: `h` hit, `p` partial, `m` miss.
Be strict; a miss simply comes back tomorrow.

**Compiling the samples.** Save a sample as `demo.cpp`, then run
`clang++ -std=c++20 -Wall -Wextra demo.cpp -o demo && ./demo`. The outputs shown
were produced with Apple clang 21 on macOS arm64 (libc++). Other compilers are
untested here; where a result depends on the library, the lesson says so.

## Pretest — guess first

### Card M1-P1 · Pretest
Q: Guess. In C++98, a function builds a local `std::vector<int>` of a million
   numbers and ends with `return v;`. What can happen to those numbers on the
   way back to the caller? Then guess what C++11 changed. One or two sentences.

<details><summary>Answer</summary>
Key:
  - C++98: the compiler is allowed to build the vector directly in the caller
    (copy elision); when it does not, the vector is copied, element by element.
  - C++11: elision is still allowed. When it does not happen, the returned local
    is moved: the caller's vector takes over the buffer, nothing is copied.
  - So C++11 did not make every return a move; it made the fallback cheap.
</details>

---

## Copy versus move

### CONCEPT C1  Lvalues, rvalues and the move constructor

**The donor nobody could name.** By 2002, C++ programmers already had tricks
for stealing a big object's insides: `swap` on containers, `list::splice`, and
the odd "copy" of `auto_ptr`. What was missing, wrote Howard Hinnant, Peter
Dimov and Dave Abrahams, was "uniform syntax and semantics to enable generic
code to move arbitrary objects".

So generic code copied. A temporary could be duplicated element by element a
moment before it was destroyed anyway, when taking its buffer would have done.

Their proposal defended copying as useful. Moving added a second way to ask,
one that means you "don't care what happens to the state of the source".

That is why the visible `std::move` at a hand-off matters: the next reader can
see that you are giving up the old value.

**The C++98 way.** C++98 had no language-defined move constructor; classes could
still implement ownership transfer through their own interfaces. To hand a big
vector over without copying, you had to ask for it by hand:

```cpp
// Fragment (source is an existing std::vector<int>); compiles with -std=c++98.
std::vector<int> result;
result.swap(source);   // O(1) exchange of internal pointers
```

**Why it hurts.** When copy elision did not apply, passing or returning these
containers by value could copy their elements.

**Three words you need.** An *lvalue* is an expression that names an object you
can refer to again, such as a variable `a`. An *rvalue* is an expression for a
temporary, or for an object you have declared you are finished with. This
property of an expression is called its *value category*. `Buffer&&` (C++11) is an
*rvalue reference*: a reference that binds only to rvalues. (`auto&&` is a
different case, met in lesson 03.)

**The modern way (C++11).** Add a *move constructor*, taking `Buffer&&`. It takes
the source's internal pointer and leaves the source valid but no longer owning
it.
`std::move(a)` moves nothing: it is a cast that turns the lvalue `a` into an
xvalue, allowing this `Buffer`'s move constructor to be selected.

```cpp
#include <cstdio>
#include <utility>
#include <vector>

struct Buffer {
    std::vector<int> data;
    explicit Buffer(int n) : data(n) {}
    Buffer(const Buffer& other) : data(other.data) { std::puts("copy"); }
    Buffer(Buffer&& other) noexcept : data(std::move(other.data)) { std::puts("move"); }
};

int main() {
    Buffer a(1000);
    Buffer b = a;             // a is an lvalue: it has a name
    Buffer c = std::move(a);  // std::move(a) is an rvalue expression
    std::printf("b=%zu c=%zu a=%zu\n", b.data.size(), c.data.size(), a.data.size());
}
```

**Predict first:** which two words print, and what is `a`'s size at the end?

<details><summary>Output (Apple clang 21, libc++)</summary>

```
copy
move
b=1000 c=1000 a=0
```

`b` got its own copy of 1000 ints. `c` took `a`'s buffer, so no ints were
copied. `a` is still a valid object; on this library its vector is empty.
</details>

**The trap.** "`std::move` makes it a move" is wrong in two common cases. If the
class has no move constructor, the copy constructor runs. If the source is
`const`, `std::move` gives `const Buffer&&`, which only the copy constructor
accepts: `const Buffer k(10); Buffer m = std::move(k);` prints `copy`.

**Rule:** in these initializations, `Buffer b = a` copies and
`Buffer c = std::move(a)` moves. Fine print: other types may copy instead or
reject the initialization.


### Card M1-C1 · Fix it
Q: Fill the four blanks to write this class's move constructor.
   ```cpp
   // Fragment: Quote is any element type; needs <string>, <utility>, <vector>.
   class MarketDataHandler {
       std::vector<Quote> quotes_;
       std::string symbol_;
   public:
       MarketDataHandler(MarketDataHandler___ other) ___
           : quotes_(std::___(other.quotes_)),
             symbol_(std::___(other.symbol_)) {}
   };
   ```
   Answer format: one line; the four blanks in order, separated by commas.

<details><summary>Answer</summary>
Key:
  - Blank 1: `&&` (the parameter is an rvalue reference).
  - Blank 2: `noexcept` (promises the move cannot throw; the next concept shows
    why `std::vector` cares).
  - Blanks 3 and 4: `move` and `move`. Inside the constructor `other` has a
    name, so `other.quotes_` is an lvalue; without `std::move` the members would
    be copied.
  - Typed answer: `&&, noexcept, move, move`
</details>

### Card M1-Q1 · Recall
Q: Two parts, one sentence each.
   (a) What does a move constructor do with the source's resources (its heap
   buffer, for example) that a copy constructor does not?
   (b) Given `Buffer a(10);`, which constructor runs for `Buffer b = a;` and
   which for `Buffer c = std::move(a);`, and why?
   Answer format: one line; separate labeled parts with semicolons.
Hint: Ask whether each source expression is an lvalue or an rvalue.


<details><summary>Answer</summary>
Key:
  - (a) For this `Buffer`, copying duplicates the vector's elements; moving
    transfers its storage. Other classes define their own copy and move behavior.
  - (b) `Buffer b = a;` runs the copy constructor because `a` is an lvalue.
    `Buffer c = std::move(a);` runs the move constructor because `std::move(a)`
    is an rvalue and `Buffer(Buffer&&)` exists and accepts it.
</details>

---

## Why `noexcept` matters to `std::vector`

### CONCEPT C2  `noexcept` and vector growth

**The situation.** When a `std::vector` is full and you add one more element, it
allocates a bigger buffer and relocates every old element into it. This is
*reallocation*.

**Why it is delicate.** For a single insertion at the end, vector preserves its
old state when the element type is CopyInsertable or nothrow move-constructible.
Fine print: if copying is unavailable and an element's move throws, the effects
can be unspecified. Suppose it moves elements
one by one and the fifth move throws. The first four sources are already
emptied, so the old state is gone. Copying does not have this problem: the old
elements are untouched until the end.

**The modern way (C++11).** `noexcept` on a function promises it will not throw.
During reallocation the vector moves an element if its move constructor is
`noexcept`; otherwise it copies it, as long as the type can be copied.

```cpp
#include <cstdio>
#include <vector>

struct A {
    A() = default;
    A(const A&) { std::puts("A copy"); }
    A(A&&) noexcept { std::puts("A move"); }
};
struct B {
    B() = default;
    B(const B&) { std::puts("B copy"); }
    B(B&&) { std::puts("B move"); }      // no noexcept
};

int main() {
    std::vector<A> va(1);
    va.reserve(2);    // grows the buffer: the one old element relocates
    std::vector<B> vb(1);
    vb.reserve(2);
}
```

**Predict first:** what do the two `reserve` calls print?

<details><summary>Output (Apple clang 21, libc++)</summary>

```
A move
B copy
```
</details>

**The trap.** "Without `noexcept` the vector always copies" is too strong. A
move-only type (no copy constructor) is still moved, and then a throw during
growth loses the strong guarantee.

**Rule:** declare a move constructor `noexcept` when it truly cannot throw (it
usually only swaps pointers), so containers can move instead of copy.


### Card M1-G3 · Spot the trap
Q: Your `OrderBook` class has a copy constructor and a move constructor, yet
   `std::vector<OrderBook>` copies every book when it grows. Name the most
   likely cause and say why the vector does this. One or two sentences.

   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - The move constructor is not declared `noexcept`.
  - For a copyable type, vector can preserve its old state if a move might
    throw: it copies the old elements instead. If copying is unavailable and a
    move throws, the effects can be unspecified.
  - Fix: add `noexcept` to the move constructor (and move assignment) when the
    implementation really cannot throw.
</details>

---

## After the move

### CONCEPT C3  What is left in a moved-from object

Moving solves the copy cost, but the source variable is still in scope. A
later log line can accidentally treat it as the old value.

**The usual library rule.** A moved-from standard-library object remains valid,
but do not assume it still has its old value. Operations must still meet their
preconditions: `if (!v.empty()) v.front();` is fine.
Fine print: some types promise a specific post-move state—`unique_ptr` becomes
null after move construction—and user-defined classes specify their own contract.

**A second surprise.** `f(std::move(v))` does not itself move anything. Whether
`v` is emptied depends on `f`. A by-value parameter is move-constructed from `v`;
an `&&` parameter is only a reference, and `f` may never move from it.

```cpp
#include <cstdio>
#include <utility>
#include <vector>

void byValue(std::vector<int> v) { std::printf("callee got %zu\n", v.size()); }
void byRef(std::vector<int>&& v) { std::printf("callee saw %zu, kept nothing\n", v.size()); }

int main() {
    std::vector<int> a(3), b(3);
    const std::size_t count = a.size();   // save it BEFORE handing a off
    byValue(std::move(a));
    byRef(std::move(b));
    std::printf("count=%zu a.size()=%zu b.size()=%zu\n", count, a.size(), b.size());
}
```

**Predict first:** what are `a.size()` and `b.size()` at the end?

<details><summary>Output (Apple clang 21, libc++)</summary>

```
callee got 3
callee saw 3, kept nothing
count=3 a.size()=0 b.size()=3
```

`a` was moved into the parameter and is empty on this library. `b` was never
moved from at all. `count` preserves `a`'s original size. `b.size()` is still
3 because `byRef` only observes `b`.
</details>

**The trap.** Two opposite wrong beliefs: "touching a moved-from object is
undefined behaviour" (no: `size()` is fine) and "it still holds its old value"
(not guaranteed).

**Rule:** read any old value you need before handing an object off. Fine print:
afterwards you may call an operation with preconditions once you establish them,
for example `if (!v.empty()) v.front();`.


### Card M1-G1 · Spot the trap
Q: Two parts.
   ```cpp
   // Fragment: Order, getOrders, processOrders and log are your own code.
   std::vector<Order> orders = getOrders();
   processOrders(std::move(orders));
   log("Processed " + std::to_string(orders.size()) + " orders");
   ```
   (a) Is calling `orders.size()` here undefined behaviour? Can you predict the
   number it logs? One sentence.
   (b) Rewrite the code so the log shows the real count. One or two lines.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Not undefined: `size()` has no precondition, so it is a valid call on a
    moved-from vector. But its value is unspecified. It may be 0 (what libc++
    shows after a move into a by-value parameter) or the original count (if
    `processOrders` takes `&&` and never moves). The log cannot be trusted.
  - (b) Save the count first:
    `const auto count = orders.size(); processOrders(std::move(orders));`
    then log `count`.
</details>

---

## Returning by value

### CONCEPT C4  Return a local by name; do not wrap it in `std::move`

Once move exists, it is tempting to force it on every return. That can
prevent the compiler from doing the even simpler thing: constructing the
result directly at its destination.

**Three mechanisms.** *Copy elision* means the compiler builds the returned
object directly in the caller's storage, so no copy or move happens. For a named
local (`return f;`) this is called *NRVO*; it has been allowed since C++98 but is
never required. Returning a temporary (`return Fills{};`) is guaranteed to be
elided since C++17.

For an eligible local of the function's return type, `return f;` permits NRVO.
Otherwise implicit-move overload resolution may select a move or a copy. This
also applies to a by-value parameter, which cannot use NRVO.

**The pessimization.** In `return std::move(f);` the returned expression is a
cast, not the local's name, so NRVO is no longer allowed. It does not guarantee
that a move constructor exists or is selected. Clang
warns: `moving a local object in a return statement prevents copy elision
[-Wpessimizing-move]`.

```cpp
#include <cstdio>
#include <utility>

struct Fills {
    Fills() = default;
    Fills(const Fills&) { std::puts("copy"); }
    Fills(Fills&&) noexcept { std::puts("move"); }
};

Fills plain()   { Fills f; return f; }             // NRVO allowed
Fills wrapped() { Fills f; return std::move(f); }  // forces a move

int main() {
    std::puts("plain:");   Fills a = plain();   (void)a;
    std::puts("wrapped:"); Fills b = wrapped(); (void)b;
}
```

**Predict first:** what prints after `plain:` and after `wrapped:`?

<details><summary>Output (Apple clang 21, -std=c++20)</summary>

```
plain:
wrapped:
move
```

`plain()` was elided: nothing printed. Build with `-std=c++11
-fno-elide-constructors` to switch elision off: `plain()` then prints `move`
(twice, one per step back to `a`), never `copy`. That is the implicit move.
</details>

**The trap.** "Never `std::move` a return value" goes too far. `return member_;`
normally copies a copyable data member, because members are not implicitly
moved. Fine print: a move-only member may make that return fail to compile. If
you mean to give the member away, `return std::move(member_);` is correct.

**Rule:** when returning a local of the function's return type by value,
prefer `return local;`. For a member you intend to give away, use `std::move`.


| Return choice | What happens | Choose it when | Smell |
|---|---|---|---|
| Copy an lvalue | Source stays usable; a second value is made | Both values are needed | A large copy appears where the source is finished |
| Move a non-const standard-library object | Source stays valid; follow the type's post-move contract | Ownership leaves this scope | Later code assumes the old value |
| `return local;` | NRVO may construct directly; otherwise move or copy may be selected | Returning an eligible local by value | `std::move(local)` appears in the return |

Advice: return a local by name, and use an explicit move only when the source
really is being handed away.

### Card M1-Q2 · Apply
Q: Two parts, one sentence each.
   ```cpp
   // Fragment: Fill, OrderBook and Order are your own types.
   std::vector<Fill> matchOrders(const OrderBook& book, const Order& incoming) {
       std::vector<Fill> fills;
       // ... matching logic fills it ...
       return fills;
   }
   ```
   (a) Compiled as C++11 or later, does this return copy the vector?
   (b) Would `return std::move(fills);` make it faster, slower, or the same?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) No. The compiler may elide it (NRVO); if it does not, `fills` is
    implicitly moved. Either way the elements are not copied. No change needed.
  - (b) Not faster: it forbids NRVO. This vector can then move, but other
    types may copy or fail to compile. Clang warns `-Wpessimizing-move`.
</details>
Stretch: Does `return p;` move when `p` is a by-value parameter, and when
  `p` is a data member?
<details><summary>Optional extension answer</summary>
StretchKey:
  - By-value parameter: NRVO does not apply; implicit-move overload resolution
    may select a move or a copy.
  - Data member: no implicit move; `return member_;` copies if the type is
    copyable and may fail otherwise. Write `std::move(member_)` if you intend
    to give it away.
</details>

### Card M1-G2 · Spot the trap
Q: A colleague ends a function with `return std::move(result);`, where
   `result` is a local variable, saying it "helps the compiler". Is that true?
   Say what actually happens, in one or two sentences.

   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - For an eligible local of the function's return type, `return result;`
    permits NRVO. Otherwise implicit-move overload resolution may select a
    move or a copy.
  - `return std::move(result);` prevents NRVO; it does not guarantee that a
    move constructor exists or is selected. Clang may warn with
    `-Wpessimizing-move`.
</details>

### Card M1-INT1 · Choose and explain
Q: Pick one place in code you know (or invent one) where a large object is
   copied on a busy path. In two or three sentences: show the line, say which
   constructor runs today, and the change that would make it a move or an
   elision.

   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - A concrete line, for example a vector of fills returned through an output
    parameter, or an order passed by value and then copied into a queue.
  - Which constructor runs now and why (the source is an lvalue, or `const`, or
    the type has no move constructor).
  - The change: return by value with `return x;`, pass by value and
    `std::move` into the member, or add a `noexcept` move constructor.
  - How you would confirm it: a copy/move trace like the samples above, or a
    benchmark on your own workload. Do not assume a speed-up you have not measured.
</details>

### Card M1-J1 · Choose and explain
Q: Today the sender thread calls `fills()` once per tick and ships the whole
   batch. Next sprint a dashboard will also call `fills()` once a second, only
   to show the count. An AI assistant wrote the member `std::move` "to avoid a
   copy". Is it right for the sender, and for the dashboard? What would you
   check first?
   Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   #include <utility>
   #include <vector>
   class FillBatch {
       std::vector<int> fills_;
   public:
       void add(int f) { fills_.push_back(f); }
       std::vector<int> fills() { return std::move(fills_); }
   };
   int main() {
       FillBatch b;
       b.add(1); b.add(2); b.add(3);
       std::vector<int> first = b.fills();
       std::vector<int> second = b.fills();
       std::printf("%zu %zu\n", first.size(), second.size());
   }
   ```

<details><summary>Answer</summary>
Key:
  - For the sender, which takes the batch away each tick, the move is right: `fills_` is a member, so plain `return fills_;` would copy it.
  - First check what every caller expects the batch to hold afterwards: calling it twice here prints `3 0`, because the member is left valid but unspecified (empty on libc++).
  - Separate consumption from observation: use `drain()` for the sender and a
    count snapshot for the dashboard. First establish how those accesses are
    synchronized; returning a const reference does not make concurrent access safe.
</details>

---

## Remember

- A named object is an lvalue; `std::move` casts it to an xvalue, which can
  select a move constructor.
- Mark a move constructor `noexcept` when it cannot throw, so `std::vector`
  moves instead of copying.
- When returning a local of the function's return type by value, prefer
  `return local;`.

Missed cards come back tomorrow. Next: lesson 02, smart pointers, where moving
is how ownership changes hands.
