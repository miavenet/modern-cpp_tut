# Lesson 01 — Saying "No Value", "One of These" and "Failed" in the Type

**Module 04 adds** the modern standard library: vocabulary types
(`optional`, `variant`, `expected`), ranges and views, structured bindings,
`std::format`, and non-owning views (`string_view`, `span`). **It assumes**
module 01 and a C++20 compiler (C++23 for `expected` and `print`). Runnable
samples are in `modules/04-stl-modern/lab/`; the build command is on line 3
of each file.

**Hook:** the lab compares two complete workloads in which half the calls
fail: one throws and catches, the other returns `expected`. Predict which
work each failure path performs, then measure both. **You will be able to:** return "maybe a value" with
`std::optional`, hold one of several message types with `std::variant`, and
return "value or error" with `std::expected`.

**Time:** about 38 minutes.
**You need:** module 01 (RAII, lambdas).

## Pretest — commit your guesses first

### Card M4-P2 · Pretest
Q: A C++98 lookup may find nothing. How do you usually return "no result",
   and what do you think C++17 `std::optional<T>` changes? Guess in one
   line.
<details><summary>Answer</summary>
Key:

  - C++98: a sentinel (`-1`, `NULL`, `npos`) or a `bool` result plus an
    out-parameter.
  - C++17: return `std::optional<T>`, which either holds a `T` inline
    without separate allocation for that storage or holds nothing, and says so in its
    type. You still must check it before using `*`.
</details>

### Card M4-P3 · Pretest
Q: In C++98, how would you store a message that is either a `Heartbeat` or
   an `Order`, and what can go wrong? Guess in one line.
<details><summary>Answer</summary>
Key:

  - A `union` plus an `enum` tag that you must keep right by hand (and
    C++98 unions allow only trivial members), or a base class with virtual
    functions used through pointers or references.
  - C++17 `std::variant<Heartbeat, Order>` normally stores one alternative inside
    itself, knows which one it holds, and runs the right destructor.
</details>

---

## std::optional

### CONCEPT C1  std::optional (C++17): a value, or explicitly nothing

**Why this exists.** A sentinel such as `-1` stops working when `-1` is
also a valid value. A `bool` plus an out-parameter makes the caller keep
two pieces in sync. An optional puts presence and value in one result.

**C++98 vs C++17, side by side** (both compile; header `<optional>`):

```cpp
// fragment: needs <map>, <optional>, <string>; full program in lab/optional_demo.cpp
// C++98: success flag plus out-parameter
bool getPrice(const std::map<std::string, double>& book,
              const std::string& symbol, double& out);

// C++17: the result says "maybe" in its type
std::optional<double> getPrice(const std::map<std::string, double>& book,
                               const std::string& symbol) {
    auto it = book.find(symbol);
    if (it == book.end()) return std::nullopt;   // "no value"
    return it->second;                           // holds a double
}
```

**Words.** An optional is *engaged* when it holds a value. `std::nullopt`
makes an empty one. `if (opt)` tests engagement; `*opt` reads the value
**without checking**; `opt.value()` checks and throws
`std::bad_optional_access` when empty; `opt.value_or(x)` gives `x` when
empty.

**See it yourself: `lab/optional_demo.cpp`.**

```
AAPL 189.5
MSFT engaged? no, value_or: 0.0
value() on empty optional threw bad_optional_access
sizeof(double)=8 sizeof(optional<double>)=16
sizeof(Point)=24 sizeof(optional<Point>)=32
```

The value lives inside the optional, plus a flag; padding rounds it up.
These sizes are this toolchain's (Apple clang, arm64). The standard does not
fix the layout, so check `sizeof` on your target.

**The trap.** "`optional` makes dereferencing safe." `*opt` on an empty
optional is undefined behaviour, just like a null pointer. The safety is
that the type tells callers to check.

**Rule:** return `std::optional<T>` for "maybe a value", and test it before
`*`, or use `value_or`.


---

### Card M4-C2 · Fix it
Q: Fill in the three blanks:
   ```cpp
// Fragment: fill the blanks in this exercise.
   std::___<std::size_t> findIndex(const std::vector<int>& v, int target) {
       for (std::size_t i = 0; i < v.size(); ++i)
           if (v[i] == target) return i;
       return std::___;
   }

   if (auto idx = findIndex(v, 42)) {
       std::cout << *___ << '\n';
   }
   ```
   Answer format: one line, the three names in order, separated by commas.
<details><summary>Answer</summary>
Key:

  - `optional, nullopt, idx`
  - `if (auto idx = ...)` tests engagement, so `*idx` is safe inside the
    `if`.
</details>

### Card M4-J1 · Apply
Q: This risk report sums open quantity per symbol; a symbol with no entry has
   no open orders. Next, the AI assistant proposes the same `value_or(0)`
   pattern for the price feed, where zero is a valid price and a missing
   quote must be reported. Is it right for each? What would you check first?
   Answer format: three short sentences: decision, first check, rejection evidence.
   ```cpp
   #include <cstdio>
   #include <map>
   #include <optional>
   #include <string>
   std::map<std::string, int> openQty{{"AAPL", 300}};
   std::optional<int> lookup(const std::string& sym) {
       auto it = openQty.find(sym);
       if (it == openQty.end()) return std::nullopt;
       return it->second;
   }
   int main() {
       int total = lookup("AAPL").value_or(0) + lookup("MSFT").value_or(0);
       std::printf("open quantity %d\n", total);
   }
   ```
<details><summary>Answer</summary>
Key:

  - For the open-quantity total, `value_or(0)` is right: no entry really
    means zero open orders, and the program prints `open quantity 300`.
  - I would first check whether the fallback is a true value in this domain
    or a disguise for "unknown".
  - For prices it is wrong, because a valid zero and a missing quote become
    the same `int`; return the optional (or an `expected`) to the caller.
</details>

---

### Card M4-Q3 · Recall
Q: A lookup can return a small value or no result. What ownership question
   does returning `optional<T>` avoid compared with returning `T*`, and what
   storage cost should you measure? Answer format: two sentences.
<details><summary>Answer</summary>
Key:

  - The optional contains the returned object, so callers need not establish
    who owns a separate pointed-to object. Measure its inline size and
    alignment; presence tracking and padding can enlarge each element.
</details>

### Card M4-G2 · Spot the trap
Q: An array of `optional<Point>` is larger than an array of `Point`. Does that
   alone prove proportionally more cache traffic? What would you measure?
   Answer format: two sentences.
<details><summary>Answer</summary>
Key:

  - No: footprint alone does not determine cache traffic. Measure both
    layouts under the actual access pattern, including how often values are absent.
</details>

---

## std::variant

### CONCEPT C2  std::variant and std::visit (C++17): one of a fixed set of types

**Why this exists.** A hand-written tag and union are two facts the
programmer must keep consistent. A variant keeps the tag with the live
object so changing the alternative changes both together.

**C++98: a tagged union.** You keep an `enum` tag next to a `union`, and
you must keep them in step. C++98 unions may hold only trivial types (no
`std::string`), and nothing stops you reading the wrong member.

```cpp
// C++98 fragment: Order and Cancel are plain structs (see lab/variant_demo.cpp)
struct Message98 {
    enum Kind { HEARTBEAT, ORDER, CANCEL } kind;   // you keep this right
    union { Order order; Cancel cancel; };          // trivial members only
};
```

**C++17: `std::variant`** (header `<variant>`) normally contains one
alternative inline and tracks which one. Changing alternatives replaces the
old object; assigning the same alternative may assign the existing object.
Fine print: a throwing change can leave it `valueless_by_exception`, in
which case `visit` throws `bad_variant_access`.

```cpp
// excerpt of lab/variant_demo.cpp
using Message = std::variant<Heartbeat, Order, Cancel>;

template <class... Fs> struct overloaded : Fs... { using Fs::operator()...; };
template <class... Fs> overloaded(Fs...) -> overloaded<Fs...>; // C++17 only

void process(const Message& m) {
    std::visit(overloaded{
        [](const Heartbeat&) { std::puts("heartbeat"); },
        [](const Order& o)   { std::printf("order %d @ %.2f\n", o.qty, o.px); },
        [](const Cancel& c)  { std::printf("cancel #%d\n", c.order_id); },
    }, m);
}
```

`overloaded` merges the lambdas into one object with three call operators.
In C++20 the deduction-guide line can go. **See it yourself:**
`lab/variant_demo.cpp` prints `order 100 @ 10.25`, `cancel #7`,
`index()=2`, `get_if<Order> returned nullptr` and
`get<Order> threw bad_variant_access`.

`visit` checks that the visitor is callable for every alternative; it does
not require a separate handler for each type. In this example, deleting the
`Cancel` handler fails because the remaining handlers cannot accept `Cancel`.
Conversions and generic handlers can hide an omitted explicit case. An
`if constexpr` chain without a final rejecting branch can silently ignore
a new type. A dependent failing `static_assert` in the fallback can enforce
explicit handling.

**Speed: variant or virtual?** Neither always wins. A
`std::vector<Message>` stores every message inline, side by side, with no
per-object allocation. A variant needs storage for its largest alternative
plus implementation-dependent discriminator and padding overhead; measure
`sizeof`. `visit` still dispatches on the stored type at run time.

Virtual functions do not need the heap (a stack object called
through a base reference works), but a container of mixed types holds
pointers, often to separately allocated objects. Benchmark your own mix.

**Rule:** use `variant` + `visit` for a closed set of types. Fine print:
`visit` checks callability, so conversions or generic handlers may hide a
missing explicit case.


---

### Card M4-C3 · Fix it
Q: Fill in the two blanks to print whatever the variant holds:
   ```cpp
// Fragment: fill the blanks in this exercise.
   std::variant<int, float> v = 42;
   std::___([](auto&& val) {
       std::cout << val;
   }, ___);
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `visit, v`
  - One generic lambda works here because both alternatives can be printed
    the same way.
</details>

### Card M4-Q5 · Recall
Q: Name two things `std::variant` does for you that a C++98 tagged `union`
   does not. Answer format: two short lines.
<details><summary>Answer</summary>
Key:

  - It records which alternative it holds (`index()`), so the "tag" cannot
    get out of step.
  - It manages construction, assignment and destruction of the active
    alternative, including non-trivial types such as `std::string`.
  - (Also accepted: reading the wrong alternative with `std::get` throws
    `std::bad_variant_access` instead of silently reading garbage.)
</details>

### Card M4-G1 · Spot the trap
Q: `msg` currently holds a `Heartbeat`. What does `std::get<Order>(msg)` do?
   Answer format: one line.
<details><summary>Answer</summary>
Key:

  - It throws `std::bad_variant_access`.
  - If you only want to test: `std::get_if<Order>(&msg)` returns `nullptr`
    when `msg` does not hold an `Order`; or handle every case with
    `std::visit`.
</details>

### Card M4-Q6 · Apply
Q: A handler keeps incoming messages of 5 types in a container and
   processes them in a loop. (a) Give one reason
   `std::vector<std::variant<...>>` with `std::visit` can be faster than a
   vector of base-class pointers with virtual calls. (b) Give one reason it
   might not be. Answer format: two short lines, (a) and (b).
<details><summary>Answer</summary>
Key:

  - (a) The messages are stored inline and next to each other: no pointer to
    follow per element and no per-object allocation, which is kinder to the
    cache.
  - (b) A variant needs storage for its largest alternative plus
    implementation-dependent discriminator and padding overhead; measure
    `sizeof`. `visit` still dispatches on the stored type. Measure the
    real message mix.
</details>

---

## std::expected

### CONCEPT C3  std::expected (C++23): a value or an error, returned normally

`expected` gives one return channel two explicit outcomes: a value or a
reason for failure. Its proposal traces the idea to Alexandrescu's 2012
error-handling talk. The useful distinction is the caller's next action:
use `optional` when absence is enough, and `expected` when the reason changes
the response.

**The C++98 choices** were an error code (easy to ignore, and the real
result goes in an out-parameter) or an exception (the failure leaves
through the unwinder, not through `return`).

**C++23: `std::expected<T, E>`** (header `<expected>`; needs `-std=c++23`
here) holds either a `T` or an error `E`. `std::unexpected(e)` makes the
error case.

```cpp
// excerpt of lab/expected_demo.cpp (C++23)
std::expected<int, ParseError> parseQty(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseError::Empty);
    int q = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), q);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::unexpected(ParseError::NotANumber);
    return q;                                    // success
}
```

**See it yourself: `lab/expected_demo.cpp`** prints `'100' -> 100`,
`'' -> error 0`, `'12x' -> error 1`, and
`value() on an error threw bad_expected_access`.

**What it does not do.** It does not force anyone to look: a call that
ignores the result compiles without a warning on this toolchain. And
`value()` on an error throws `std::bad_expected_access`. Test with
`if (r)` and then use `*r` or `r.error()`.

**The cost of throwing.** `lab/throw_cost.cpp` compares complete workloads
with half the calls failing. It includes exception construction and the
return path, so its average is not isolated failure latency. Run it on your
target before making a performance claim.

**Rule:** when failure is a normal, frequent outcome, return it as a value
with `std::expected` and check it before use.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| `optional<T>` | Holds value or absence, no reason | A miss needs no explanation | Callers invent a second error channel |
| `expected<T,E>` | Holds value or typed error | Failure reason changes the response | Error is ignored or always mapped to one default |
| Exception | Transfers control by throwing | Failure is exceptional in this API | Routine misses repeatedly unwind |
| Error code plus out-parameter | Caller must keep status and output together | Existing ABI requires it | Output is read after failed status |

Advice: choose the result by what the caller must do with failure; a
magic fallback value is the smell that the return type is too weak.


---


### Card M4-Q4 · Apply
Q: Write a C++23 function returning `std::expected<int, ParseError>` that
   returns `ParseError::invalid` on invalid input and `42` otherwise. Show
   a caller handling both outcomes without `.value()`. Assume `invalid` is
   a value of enum `ParseError`, and `valid` is a bool parameter.
   Answer format: function and caller code, then one sentence on why this
   suits routine parse failures.
<details><summary>Answer</summary>
Key:

  - `std::expected<int, ParseError> parse(bool valid) { if (!valid) return
    std::unexpected(ParseError::invalid); return 42; }`
  - `auto r = parse(valid); if (r) std::printf("%d\n", *r); else
    std::printf("error %d\n", int(r.error()));` (with `<cstdio>`).
  - Prefer `expected` here: routine parse failure returns through normal
    control flow. The supplied benchmark compares complete workloads; it
    does not isolate unwinder cost or establish your application's latency.
</details>

---

### Card M4-Q12 · Recall
Q: What does `std::expected<int, ParseError>` hold after a routine parse
   failure, and how does a caller inspect it without throwing?
   Answer format: one sentence.
<details><summary>Answer</summary>
Key:

  - It holds a `ParseError`; the caller tests `if (r)` and reads `r.error()`
    on failure, or `*r` on success.
</details>

---

> Extra practice (optional): Card M4-PB4 in lesson-06-practice-bank.

---

## Closing

Remember: `optional` says "maybe", but `*` still needs a check. `variant` +
`visit` checks callability for every alternative; check for conversions
that could hide a missing explicit case.
`expected` returns failure as a value; check it. Misses come back
tomorrow. Next: lesson 02, ranges and views.
