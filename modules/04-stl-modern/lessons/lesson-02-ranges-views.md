# Lesson 02 — Ranges and Views: Pipelines That Do Work Only When Asked

**Hook:** `std::sort(a.begin(), b.end())` mixes two different vectors and
compiles without a single warning. `std::ranges::sort(a)` cannot make that
mistake. **You will be able to:** call range algorithms, build a lazy
`filter | transform` pipeline, predict when it runs, avoid the two view
traps (const iteration and dangling), and make your own type work with
`std::ranges`.

**Time:** about 29 minutes. **You need:** lambdas (module
01) and STL iterators.

## Pretest — commit your guess first

### Card M4-P1 · Pretest
Q: You must take the active orders from a vector, extract their prices, and
   keep the first 10. A straightforward implementation materializes filtered
   orders in one vector and their prices in another. Which intermediate
   containers can the view pipeline avoid? Guess in one line.
<details><summary>Answer</summary>
Key:

  - It avoids both materialized intermediates in that implementation.
    Classic algorithms do not inherently allocate vectors; their output
    iterators determine where results go.
  - A C++20 pipeline,
    `orders | views::filter(isActive) | views::transform(getPrice) | views::take(10)`,
    produces elements as you iterate.
</details>

---

## Range algorithms

### CONCEPT C4  Range algorithms (C++20): pass the container, not two iterators

**"Blech!"** In November 2014 Eric Niebler called the committee meeting in
Urbana-Champaign "a watershed moment" for his ranges work; Herb Sutter
described the mood in the room as "palpable excitement".

But Niebler had found a hole in his own proposal. To read some ints, sort them
and drop duplicates, you still wrote the old dance: `std::sort(ints)`,
`auto i = std::unique(ints)`, then `ints.erase(i, ints.end())`. His verdict:
"Blech!"

Two kinds of algorithm did not meet. The standard ones "operate eagerly" and
change data now. Views are lazy and compose freely ("filter a transformed
slice? No problem!") but change nothing. His range-v3 library became the basis
for C++20 ranges.

This lesson keeps that distinction: sorting changes a container now; a view
describes elements to produce later.

**Words first.** A *range* is anything with a beginning and an end you can
iterate: a container, an array, a view. The C++20 range algorithms live in
namespace `std::ranges` (header `<algorithm>`) and take the range itself.

```cpp
// fragment: prices, a and b are std::vector<double>; needs <algorithm>
// C++98: two iterators that must belong to the same container
std::sort(prices.begin(), prices.end());
std::sort(a.begin(), b.end());   // wrong container: compiles, undefined behaviour

// C++20: one argument, nothing to mismatch
std::ranges::sort(prices);
```

The mismatched line compiles with `-Wall -Wextra` and no warning on this
toolchain; at run time it is undefined behaviour.

**Also new:** range algorithms state their requirements as concepts
(module 02), such as `std::ranges::random_access_range`, so a wrong type
fails with a named requirement instead of an error deep inside the
library.

**The trap.** `std::sort(v)` does not exist. The range form is
`std::ranges::sort(v)`.

**Rule:** prefer `std::ranges::` algorithms whenever you mean "the whole
container".


---

### Card M4-C4 · Fix it
Q: Fill in the two blanks to sort the whole vector with a C++20 range
   algorithm:
   ```cpp
// Fragment: fill the blanks in this exercise.
   std::vector<int> v = {3, 1, 4};
   std::___::___(v);
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `ranges, sort`
  - `std::ranges::sort(v)` takes the whole range, so the begin/end pair
    cannot be mismatched.
</details>

---

## Views

### CONCEPT C5  Views (C++20): lazy steps you chain with `|`

**Why this exists.** An eager filter followed by a transform often needs
intermediate storage or a hand-written fused loop. A view lets you name
the stages and pull only the elements a consumer requests.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| Eager algorithm | Work finishes now; result can be owned | You need a stable stored result | Repeated passes rebuild needless intermediates |
| Lazy view | Work happens during iteration; may borrow input | You stream through data with a live owner | Returned view outlives its local input |
| Hand-written loop | Explicit control and storage | A measured hot loop needs special control | Logic duplicates a clear pipeline without evidence |

Advice: choose a view when the input stays alive through consumption;
iteration after the owner dies is the smell that you needed an owning result.

**Words first.** A *view* is a lightweight range that presents elements of
another range, changed on the fly. `std::views::filter(pred)` keeps
elements where `pred` is true; `std::views::transform(f)` presents `f(x)`;
`std::views::take(n)` stops after `n` (header `<ranges>`). The `|` chains
them. *Lazy* means: nothing runs when you build the pipeline; each element
is computed when the loop asks for it.

```cpp
// fragment: orders is a std::vector<Order>, Order has bool isActive and double price
// C++98: one hand-written loop (also compiles with -std=c++98)
std::vector<double> firstTen;
for (std::vector<Order>::const_iterator it = orders.begin();
     it != orders.end() && firstTen.size() < 10; ++it)
    if (it->isActive) firstTen.push_back(it->price);

// C++20: the same steps as a pipeline, no vector filled
auto firstTen20 = orders
    | std::views::filter([](const Order& o) { return o.isActive; })
    | std::views::transform([](const Order& o) { return o.price; })
    | std::views::take(10);
for (double p : firstTen20) std::printf("%.0f ", p);
```

**See it yourself: `lab/lazy_views.cpp`** counts the filter's predicate
calls over `{1, 2, 3, 4, 5, 6}`:

```
after building the pipeline: 0 predicate calls
4 16 36
after first loop: 6 predicate calls
4 16 36
after second loop: 10 predicate calls
```

Building it called nothing. Each loop runs the predicate again. Why 4, not
6, the second time? See the next concept.

**Performance.** The pipeline creates no intermediate container. It is not
"no work": building and walking the views runs your lambdas, and they may
allocate. Optimised pipeline code is often close to the hand-written loop,
but not always; compare with `-O2 -S` or a benchmark before you rely on it.

**Rule:** a view is a recipe; its work happens as you iterate it.


---

### Card M4-C1 · Fix it
Q: Fill in the two blanks so the pipeline keeps the even numbers and squares
   them:
   ```cpp
// Fragment: fill the blanks in this exercise.
   std::vector<int> nums = {1, 2, 3, 4, 5};
   auto result = nums
       | std::views::___([](int n) { return n % 2 == 0; })
       | std::views::___([](int n) { return n * n; });
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `filter, transform`
  - Iterating `result` yields 4 and 16; nothing is computed before that.
</details>

### Card M4-Q1 · Recall
Q: What does it mean that a `std::views` pipeline is "lazy"? When do its
   lambdas actually run? Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - Building the pipeline runs none of its lambdas and fills no container.
  - They run while you iterate it (a range-`for`, an algorithm, or copying
    into a container), and again on each new iteration.
</details>

### Card M4-Q2 · Apply
Q: `auto v = vec | std::views::filter(pred);` (a) Does this line allocate a
   new vector? (b) Is iterating `v` guaranteed to be as fast as a
   hand-written `for` loop with an `if`? Answer format: two short lines,
   (a) and (b).
<details><summary>Answer</summary>
Key:

  - (a) No. The view refers to `vec` and stores the predicate; no container
    is filled.
  - (b) No guarantee. It is often close after optimisation, but it depends
    on the pipeline, compiler and data; check the generated code or
    benchmark both.
</details>

---

### CONCEPT C6  Two view traps: cached begin and dangling

**Why this matters.** Laziness postpones work, and postponed work also
postpones reads from the source. A view that borrows a local can look fine
at construction and fail only when someone finally iterates it.

**Trap 1: `filter_view` caches `begin()` over a forward range.** Over a
forward range such as this vector, `filter_view` caches its first matching
iterator. That explains the reduced predicate-call count on the second
traversal. A single-pass input range does not have this caching requirement.
Two consequences:

- A `filter_view` over a vector cannot be iterated through a `const`
  reference (C++20; the current draft adds a `const begin()` only for
  single-pass ranges):

```cpp
// fragment: square and isEven are int -> int / int -> bool functions
void print_all(const auto& r) { for (int x : r) std::printf("%d ", x); }

print_all(nums | std::views::transform(square)); // OK
print_all(nums | std::views::filter(isEven));    // error
```

  Clang: `'this' argument to member function 'begin' has type 'const
  std::ranges::filter_view<...>', but function is not marked const`.
  Take views by forwarding reference (`auto&& r`) or by value instead.
  `auto&&` preserves the caller's constness; it does not make an already-const,
  non-const-iterable view iterable.
- Do not modify an element through the view so that it stops matching
  the predicate: the standard says that can be undefined behaviour.

**Trap 2: borrowing versus owning.** `nums | views::filter(...)` on a named
vector *borrows* it (it wraps a `ref_view`, a pointer to `nums`). On an
rvalue, such as `std::move(nums)`, it *owns* it (an `owning_view`, which
holds the moved vector). `lab/dangling.cpp` shows both:

```cpp
// excerpt of lab/dangling.cpp
auto pricesAbove100_borrowing() {
    std::vector<double> prices{100.0, 101.5, 99.0, 102.0};
    return prices | std::views::filter([](double p) { return p > 100; });
}   // prices dies here: the returned view dangles
```

The owning version (`std::move(prices) | ...`) prints `101.5 102.0`. The
borrowing one compiles without a warning; with `-DSHOW_BUG
-fsanitize=address` it stopped with an AddressSanitizer error.

**Algorithms protect you on temporaries.** `std::ranges::find(make(), 4)`,
where `make()` returns a vector by value, returns `std::ranges::dangling`
instead of an iterator, so `*it` fails to compile (`indirection requires
pointer operand`). A *borrowed range*, such as `std::span`, does not own its
elements. Destroying the span object does not itself invalidate its
iterators, but the underlying elements must still live; for such a range
the algorithm returns a real iterator.

**Rule:** take views by `auto&&`, and never return a view that borrows a
local.


---

### Card M4-J2 · Apply
Q: A reviewer quotes "never return a view that borrows" and rejects this
   AI-written member function. Today every caller iterates it at once on a
   `Book` it holds. A new caller proposes `auto bids = loadBook().activeBids();`
   and iterates `bids` on the next line. Is the function right? What would you
   check first? Answer format: three short sentences: decision, first check, rejection evidence.
   ```cpp
   #include <cstdio>
   #include <ranges>
   #include <vector>
   class Book {
       std::vector<int> bids_{100, 0, 250, 0, 50};
   public:
       auto activeBids() const {
           return bids_ | std::views::filter([](int q) { return q > 0; });
       }
   };
   int main() {
       Book book;
       for (int q : book.activeBids()) std::printf("%d ", q);
       std::puts("");
   }
   ```
<details><summary>Answer</summary>
Key:

  - For today's callers it is right: the view borrows the member `bids_`,
    which lives as long as the `Book`, and this prints `100 250 50`.
  - I would first check, at every call site, that the `Book` outlives the
    last iteration of the view.
  - The new caller changes the verdict: the temporary `Book` dies at the end
    of that statement, so later iteration of `bids` reads a destroyed vector.
</details>

---

### Card M4-G3 · Spot the trap
Q: What is wrong with this function, and give one fix?
   ```cpp
   auto getPrices() {
       std::vector<double> prices = {100.0, 101.5, 99.0};
       return prices | std::views::filter([](double p){ return p > 100; });
   }
   ```
   Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `prices` is a named local, so the filter view only borrows it (through a
    `ref_view`). After the return, `prices` is destroyed and iterating the
    view is undefined behaviour.
  - Fix: return an owning result, e.g. `std::move(prices) | std::views::filter(...)`
    (the view then owns the vector), or copy the matches into a vector you
    return.
</details>

---

## Your own types and std::ranges

### CONCEPT C7  Making a type range-compatible, and iterator categories

**Why this exists.** Generic algorithms need to know which operations an
iterator can actually perform. `reverse` needs a step backward; giving a
forward-only iterator a stronger label would make the algorithm's promise
false.

**What makes a range.** A type is a `std::ranges::range` when
`std::ranges::begin(r)` and `std::ranges::end(r)` both work. The usual way
is member `begin()` and `end()`; free functions `begin`/`end` found by
argument-dependent lookup also work. The end may be a different type (a
*sentinel*). `size()` is not required.

**What an algorithm or view needs** depends on the iterator's *category*:

| Category | You can | Example |
|---|---|---|
| input | read each element once, going forward | reading a stream |
| forward | read again from a saved position (multi-pass) | `std::forward_list` |
| bidirectional | also step backwards (`--it`) | `std::list` |
| random access | also jump (`it + n`) and subtract | `std::vector` |

**See it yourself: `lab/custom_range.cpp`** prints `105 103 101`:

```cpp
// excerpt of lab/custom_range.cpp (the loop body prints p)
class OrderBook {                              // wraps a vector of prices
    std::vector<int> prices_{105, 101, 103};
public:
    auto begin() { return prices_.begin(); }   // found by ranges::begin
    auto end()   { return prices_.end(); }
};
static_assert(std::ranges::random_access_range<OrderBook>);

std::ranges::sort(book);                       // needs random access
for (int p : book | std::views::reverse) ...   // needs bidirectional
```

`std::views::reverse` on a `std::forward_list` fails to compile (`invalid
operands to binary expression`), because a forward iterator cannot step
back.

**The trap.** Thinking `size()` or inheriting from a library base is
needed. Only `begin`/`end` and the iterator's category matter.

**Rule:** give your type `begin()`/`end()` returning iterators of the
strongest category you can honestly support, and check it with a
`static_assert` on the range concept.


---


### Card M4-Q7 · Recall
Q: What must your `OrderBook` class minimally provide so that
   `std::ranges::begin(book)` and `std::ranges::end(book)` work, making it a
   `std::ranges::range`? Answer format: one sentence.
<details><summary>Answer</summary>
Key:

  - `begin()` and `end()`, usually as member functions (free functions found
    by argument-dependent lookup also work), returning an iterator and a
    matching end (a sentinel, which may be a different type). `size()` is
    not required.
</details>

### Card M4-Q8 · Apply
Q: You want `std::views::reverse` on your container. Which iterator category
   must its iterators at least have? Answer format: one word.
<details><summary>Answer</summary>
Key:

  - Bidirectional (`std::bidirectional_iterator`): reversing needs `--it`.
    `views::reverse` cannot wrap a `forward_list`, because its iterators are
    not bidirectional. `forward_list::reverse()` can reverse the links in place.
</details>

---

## Closing

Remember: `std::ranges::` algorithms take the whole range; a view is a lazy
recipe that does work as you iterate; a filter view over a forward range
caches `begin()`, so
take views by `auto&&`; never return a view that borrows a local. Misses
come back tomorrow. Next: lesson 03, structured bindings and
`std::format`.
