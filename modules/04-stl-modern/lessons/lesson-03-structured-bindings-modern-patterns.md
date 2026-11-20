# Lesson 03 — Structured Bindings, if-init and std::format

**Hook:** `for (auto [symbol, book] : books) book.update();` compiles,
runs, and updates nothing in `books`: every `book` is a copy. **You will be
able to:** unpack pairs and structs into named variables without accidental
copies, scope a lookup to its `if`, and format text without `<<` chains,
knowing when it allocates.

**Time:** about 26 minutes. **You need:**
lessons 01–02 of this module (the synthesis card uses a view).

## Pretest — commit your guess first

### Card M4-P4 · Pretest
Q: In C++98, after `m.insert(...)` on a `std::map`, how do you get the
   iterator and the "was it inserted?" flag? What might C++17 make shorter?
   Guess in one line.
<details><summary>Answer</summary>
Key:

  - C++98: store the returned pair and use `.first` and `.second`:
    `std::pair<std::map<K, V>::iterator, bool> res = m.insert(...); if (res.second) ...`
  - C++17: a structured binding names both parts at once:
    `auto [it, inserted] = m.insert(...);`
</details>

---

## Structured bindings

### CONCEPT C8  Structured bindings (C++17): names for the parts, copy or reference

**"declare-and-tie".** By 2016, returning several values was, in the words of
Herb Sutter, Bjarne Stroustrup and Gabriel Dos Reis, "a popular request
lately". The return side already looked fine: `return {a,b,c};` into a `tuple`.

The call side did not. `tie(x,y,z) = f();` needed `x`, `y` and `z` declared
first. Class objects often had to be default-constructed and then assigned
their returned values; scalar variables could be left uninitialized before
assignment. Their example: a function that opens a file stream
and returns it with a status, where even default-constructing the stream is
"often undesirable".

They sketched the wish as `declare-and-tie(x,y,z) = f();`. It became
`auto [x,y,z]`, square brackets replacing an earlier draft's braces.

One catch comes with that design: there is one hidden object, and your `auto`,
`auto&` or `const auto&` decides whether it is copied or referred to.

**C++98 vs C++17** (both compile):

```cpp
// fragment: m is a std::map<std::string, double>
// C++98
std::pair<std::map<std::string, double>::iterator, bool> res =
    m.insert(std::make_pair(std::string("AAPL"), 150.0));
if (res.second) std::printf("inserted %s\n", res.first->first.c_str());

// C++17
auto [it, inserted] = m.insert({"AAPL", 150.0});
if (inserted) std::printf("inserted %s\n", it->first.c_str());
```

**What the declaration really does.** `auto [a, b] = expr;` first makes one
hidden object, initialised from `expr`, and then `a` and `b` name its
parts. It works on pairs, tuples, plain structs with public members, and
arrays. For an ordinary named map entry, the choice is:

| You write | Hidden object |
|---|---|
| `auto [k, v] = x;` | a **copy** of the named map entry `x` |
| `auto& [k, v] = x;` | a reference to `x` |
| `const auto& [k, v] = x;` | a const reference to `x` |

Fine print: a fresh returned value can construct the hidden object directly,
and reference-valued tuple components can still refer outside it.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| `auto [a,b]` | Makes its own object | Unpacking a fresh return value | Mutation of a container element disappears |
| `auto& [a,b]` | Refers to the source | You must change an existing element | A copy counter climbs in a write loop |
| `const auto& [a,b]` | Refers without allowing changes | Read-only loop over existing elements | Large objects copied for inspection |

Advice: in a container loop, decide whether you need ownership or a
reference before choosing `auto`; an update that vanishes is the smell.

**See it yourself: `lab/bindings.cpp`.** A `Book` counts its copies:

```
by value:     copies=2, AAPL updates=0
by reference: copies=2, AAPL updates=1
inserted AAPL again? no
```

The by-value loop copied both map entries and updated the copies, so the
map saw nothing. The `auto&` loop made no new copies and the update stuck.

**The trap.** Reading `auto [symbol, book]` as "two names for the map's
data". It is two names for a copy.

**Rule:** in loops over containers, write `auto&` (to change) or
`const auto&` (to read).


---

### Card M4-J3 · Apply
Q: A pricing tool shows what total fees would be after a 10% rise, and must
   not change the live fee table. A reviewer flags the AI-written by-value
   binding as "the vanishing update bug". Is it right here? What would you
   check first, and what would change your answer? Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   #include <map>
   #include <string>
   int main() {
       std::map<std::string, double> fees{{"AAPL", 1.0}, {"MSFT", 2.0}};
       double whatIfTotal = 0;
       for (auto [symbol, fee] : fees) {   // copies each entry
           fee *= 1.10;                    // try a 10% rise
           whatIfTotal += fee;
       }
       std::printf("what-if %.2f, live AAPL %.2f\n", whatIfTotal, fees.at("AAPL"));
   }
   ```
<details><summary>Answer</summary>
Key:

  - By value is right here: the copy is exactly what leaves the live table
    alone, and the program prints `what-if 3.30, live AAPL 1.00`.
  - I would first check whether the loop's job is to change the container or
    only to compute from it.
  - If the task were to apply the rise, this loop would be the bug and
    `auto&` would be required; for a large value type, `const auto&` plus a
    local copy of the one field would avoid copying whole entries.
</details>

---

### Card M4-G4 · Spot the trap
Q: What is wrong here, and what is the fix?
   ```cpp
   std::map<std::string, OrderBook> books;
   for (auto [symbol, book] : books) {
       book.update();
   }
   ```
   Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `auto [symbol, book]` copies each map entry (the string and the whole
    `OrderBook`) every iteration, and `update()` changes the copy, so the
    map is never updated.
  - Fix: `for (auto& [symbol, book] : books)`; use `const auto&` when you
    only read.
</details>

---

## if with an initialiser

### CONCEPT C9  if-init statements (C++17): keep the lookup inside the `if`

**Why this exists.** A lookup iterator is useful for one branch, yet a
wide scope lets stale names linger after that branch. The initializer
ties the name to the test that justifies using it.

**C++98:** the iterator outlives the test and leaks into the rest of the
function, where someone may reuse it by mistake.

```cpp
// fragment: m is a std::map<std::string, double>
// C++98
std::map<std::string, double>::iterator it = m.find("AAPL");
if (it != m.end()) std::printf("%.1f\n", it->second);
// 'it' is still in scope here

// C++17: 'it' exists only inside the if and its else
if (auto it = m.find("AAPL"); it != m.end()) {
    std::printf("%.1f\n", it->second);
}
```

The form is `if (init; condition)`. C++17 also allows it on `switch`, and
C++20 on range-`for`.

**The trap.** Expecting the variable after the `if`. It is gone; that is the
point.

**Rule:** when a variable exists only to be tested, declare it in the
`if`.


---

### Card M4-INT1 · Choose and explain
Q: `orders` is a `std::map<std::string, std::vector<Order>>`, and `Order`
   has `int id` and `bool isActive`. Write the body of
   `printActive(orders, symbol)`: if `symbol` is in the map, print the id
   of each active order, using an if-init, a structured binding that does
   not copy, and `std::views::filter`. Answer format: code on one line, or
   type `:skip` and compare with the key.
<details><summary>Answer</summary>
Key:

  - A version that compiles with `-std=c++20`:
    ```cpp
    if (auto it = orders.find(symbol); it != orders.end()) {
        const auto& [sym, list] = *it;   // no copy
        for (const Order& o : list | std::views::filter([](const Order& x) { return x.isActive; }))
            std::printf("%s order %d\n", sym.c_str(), o.id);
    }
    ```
  - Check your three parts: `if (init; cond)`, `const auto&` (or `auto&`)
    on the binding, and the filter view in the loop.
</details>

---

## Formatting text

### CONCEPT C10  std::format (C++20) and std::print (C++23)

**Why this exists.** Formatting with `printf` makes the format string and
arguments a separate agreement; stream chains avoid that agreement but
spread one message across several operations. `std::format` puts a checked
format and its arguments in one call.

**C++98 choices:** `printf` (fast to write, no type checking of arguments
against the format) or `std::ostream` with `<<` chains (type-safe,
verbose, formatting state that sticks to the stream).

**C++20 `std::format`** (header `<format>`) uses Python-style `{}` fields
and checks the format string against the argument types at compile time.
**C++23 `std::print`** (header `<print>`; `-std=c++23` here) writes
straight to the output.

```cpp
// fragment: price is a double, qty an int
std::string s = std::format("Price: {:.2f}, Qty: {}", price, qty);
std::print("Price: {:.2f}, Qty: {}\n", price, qty);   // C++23
```

**Where it allocates.** `std::format` returns a `std::string`. libc++ keeps
up to 22 characters inside the string object; longer text goes on the
heap. `lab/alloc.cpp` counts it: a 23-character result made 1 allocation.
For a fixed buffer, use `std::format_to_n` with the buffer's capacity and
check the reported size (`lab/fmt.cpp`):

```cpp
// excerpt of lab/fmt.cpp
char buf[16];
auto r = std::format_to_n(buf, sizeof buf - 1, "Price: {:.2f}, Qty: {}", price, qty);
*r.out = '\0';
// prints: buffer holds "Price: 101.26, "; full text needed 23 chars
```

`format_to` with a plain pointer does not know the buffer size, so it can
overflow.

**About `std::cout` and threads.** Using `std::cout` from several threads
is not a data race (the standard streams are synchronised by default), but
the characters can interleave. C++20 `std::osyncstream` groups output; on
this toolchain it needs `-fexperimental-library`.

**Rule:** use `std::format` when a returned string suits the call site.
Fine print: `format_to_n` bounds destination writes, not total work or internal
allocations. Check truncation. For a no-allocation or strict-latency path,
verify the actual formatters and library implementation, or use measured
`to_chars` and explicit buffer handling.


---


### Card M4-G5 · Spot the trap
Q: You call `std::format` for every order inside the matching engine's hot
   loop. What cost might that add, and how would you bound destination writes?
   Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `std::format` returns a new `std::string`; text longer than the
    small-string buffer (22 characters in libc++) is heap-allocated, which
    adds allocation latency per call.
  - `format_to_n` bounds destination writes, not total work or internal
    allocations. Check truncation. For a no-allocation or strict-latency path,
    verify the actual formatters and library implementation, or use measured
    `to_chars` and explicit buffer handling.
</details>

### Card M4-Q11 · Recall
Q: In a loop over map entries, which binding changes the original entry:
   `auto [k, v]` or `auto& [k, v]`? What does `format_to_n` bound?
   Answer format: two short sentences.
<details><summary>Answer</summary>
Key:

  - `auto& [k, v]` binds the original map entry; plain `auto` makes a copy
    of that named entry.
  - `format_to_n` bounds writes to the destination, not total work or
    internal allocations.
</details>

### Card M4-INT2 · Choose and explain
Q: A C++98 handler stores messages as `struct Msg { enum Kind kind; union
   { Order order; Cancel cancel; }; }` and `switch`es on `kind`. Describe
   the C++17 rewrite in one or two lines, and name the one measurement that
   decides whether it was worth it. Answer format: two short lines.
<details><summary>Answer</summary>
Key:

  - `using Msg = std::variant<Heartbeat, Order, Cancel>;` and
    `std::visit(overloaded{...}, msg)` with one lambda per type (lesson 01):
    the tag cannot get out of step. `visit` checks callability for every
    alternative, but conversions can hide a missing explicit handler.
  - Measure the handler's latency or throughput on a realistic message mix,
    before and after; also check `sizeof(Msg)`, since a variant needs storage
    for its largest alternative plus discriminator and padding overhead.
</details>

---

> Extra practice (optional): Cards M4-PB5 and M4-PB6 in lesson-06-practice-bank.

---

## Closing

Remember: `auto [a, b]` creates a hidden object from its initializer: a named
ordinary map entry is copied, while a fresh returned value can be constructed
directly. Reference-valued components can still refer to external objects.
Use `auto&` to bind to the original map entry. Put a tested variable in the
`if`. `format_to_n` bounds destination writes, not total work. Misses come
back tomorrow. Next: lesson 04, non-owning views of strings and buffers.
