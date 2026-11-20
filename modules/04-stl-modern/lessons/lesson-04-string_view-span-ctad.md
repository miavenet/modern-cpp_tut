# Lesson 04 — Non-Owning Views (string_view, span) and Two Conveniences

**Hook:** a `string_view` of `"12"` handed to `atof` came back as
`121275`. The view was right; the C function read past its end.
**You will be able to:** pass text and buffers without copying, using
`std::string_view` and `std::span`, avoid the three ways views dangle,
parse a view safely, let CTAD deduce template arguments, and use
`[[nodiscard]]`.

**Time:** about 34 minutes. **You need:** lessons 01–03
of this module.

## Pretest — commit your guesses first

### Card M4-P5 · Pretest
Q: A function takes `const std::string&`, and you call it with a string
   literal—an ordinary string literal is an array of `const char` that can
   convert to `const char*`. What happens before the function runs? What
   might C++17 offer instead? Guess in one line.
<details><summary>Answer</summary>
Key:

  - A temporary `std::string` is constructed: the characters are copied,
    and if the text is longer than the small-string buffer (22 characters
    in libc++) it is also heap-allocated.
  - C++17 `std::string_view` takes a pointer and a length instead: no copy,
    no allocation.
</details>

### Card M4-P6 · Pretest
Q: In C++98, how do you pass "some contiguous bytes" (from an array or a
   vector) to a function without making it a template? What might C++20
   change? Guess in one line.
<details><summary>Answer</summary>
Key:

  - C++98: a pointer and a length, `void process(const unsigned char* data,
    size_t len)`, which nothing checks belong together.
  - C++20: `std::span<const T>` bundles the pointer and length into one
    non-owning view that a vector or an array of contiguous matching elements
    converts to.
    For `std::byte` elements, `std::span<const std::byte>` does that job.
</details>

---

## std::string_view

### CONCEPT C11  std::string_view (C++17): a pointer and a length, nothing more

**Why this exists.** A parser may receive a literal, a `std::string`, or
one field inside a larger message. Requiring a `std::string` object for
every call can create a copy; a pointer alone loses the field's length.
The view carries both without owning either.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| `string_view` | No ownership; length is explicit | Read text while the owner lives | Stored beyond the owner's lifetime |
| `const string&` | Refers to a string object | Caller already has a string and API needs it | Literals require temporary strings on a hot path |
| `string` by value | Owns its characters | Callee must retain the text | Copies appear for read-only calls |

Advice: use `string_view` for a bounded read during the call; a returned
view of local text is the smell that the result must own its bytes.

**C++98 vs C++17:**

```cpp
// two alternative declarations, not one program
// C++98
void parseOrder(const std::string& fixMsg);
parseOrder("8=FIX.4.2|9=65|35=D|49=CLIENT|");  // builds a temporary std::string

// C++17 (header <string_view>)
void parseOrder(std::string_view fixMsg);
parseOrder("8=FIX.4.2|9=65|35=D|49=CLIENT|");  // passes pointer + length
```

**See it yourself.** `lab/alloc.cpp` counts heap allocations on this
toolchain: a 15-character literal passed as `const std::string&` made 0
(it fits the 22-character buffer inside the string object); a 30-character
one made 1. A `string_view` never allocates.

`lab/views_sv.cpp` slices without copying:

```cpp
// excerpt of lab/views_sv.cpp
std::string msg = "35=D|44=1275|38=100|";
std::string_view whole = msg;                   // no copy
std::string_view price = whole.substr(8, 4);    // "1275", still no copy
int px = 0;
auto [end, ec] = std::from_chars(price.data(), price.data() + price.size(), px);
// px == 1275; from_chars reads exactly the 4 characters
```

**The contract.** A `string_view` does not own its characters:

1. The characters must outlive the view, or the view *dangles* (points at
   freed memory).
2. Its end is not guaranteed to be followed by `'\0'`. A C function that
   reads until `'\0'` ignores the view's length. `lab/atof_bug.cpp`: the
   view `"12"` into `"121275"` gives `atof(qty.data()) = 121275`, while
   `std::from_chars` gives `12`. If the bytes have no `'\0'` after them at
   all (a raw network buffer), such a C function reads past the buffer:
   undefined behaviour.

`std::from_chars` (C++17, header `<charconv>`) takes a begin and an end,
throws nothing, and reports errors in its result. Floating-point
`from_chars` works on this toolchain.

**The trap.** Treating `sv.data()` as a C string.

**Rule:** take text parameters as `std::string_view`, keep the owner
alive, and parse with length-aware functions.


---

### Card M4-J4 · Apply
Q: Today the venue table is filled once at start-up from string literals.
   Next month venues will come from a config file, read line by line into one
   reused `std::string`. An AI assistant stores `std::string_view` in the table
   "to avoid copies". Is it right now, and after the change? What would you
   check first? Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   #include <string_view>
   #include <vector>
   struct Venue { std::string_view name; int id; };
   std::vector<Venue> venues;
   void registerVenues() {
       venues.push_back({"XNAS", 1});      // string literals live for the whole program
       venues.push_back({"XNYS", 2});
   }
   int main() {
       registerVenues();
       for (const Venue& v : venues) std::printf("%.*s=%d\n", (int)v.name.size(), v.name.data(), v.id);
   }
   ```
<details><summary>Answer</summary>
Key:

  - It is right today: string literals live for the whole program, so the
    stored views never dangle, and this prints `XNAS=1` and `XNYS=2`.
  - I would first check where each view's characters live and whether that
    storage outlives the table.
  - The config-file change flips the verdict: every stored view would point
    into one line buffer that is overwritten or destroyed, so the table must
    then hold `std::string`.
</details>

---

### Card M4-C5 · Fix it
Q: Fill in both blanks so the tag is passed without a copy:
   ```cpp
// Fragment: fill the blanks in this exercise.
   void parseTag(std::___ tagData) {
       // ... read tag ...
   }

   std::string fixMsg = "35=D|49=CLIENT|";
   parseTag(std::___(fixMsg.data(), 4)); // pass "35=D"
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `string_view, string_view`
  - The second is the pointer-plus-length constructor:
    `std::string_view(fixMsg.data(), 4)`.
</details>

### Card M4-G6 · Spot the trap
Q: What is wrong with this function? Answer format: one or two sentences.
   ```cpp
   std::string_view getBrokerName() {
       std::string name = "BROKER_A";
       return name;
   }
   ```
<details><summary>Answer</summary>
Key:

  - The returned view points at the characters of the local `name`, which
    is destroyed when the function returns: the view dangles, and reading
    through it is undefined behaviour.
  - Clang warns here: `address of stack memory associated with local
    variable 'name' returned`. Fix: return `std::string`.
</details>

### Card M4-G7 · Spot the trap
Q: What is wrong with this code (`using namespace std::string_literals;` is
   in effect)? Answer format: one or two sentences.
   ```cpp
   std::string_view sv = "hello"s + " world"s;
   std::cout << sv;
   ```
<details><summary>Answer</summary>
Key:

  - `"hello"s + " world"s` is a temporary `std::string`, destroyed at the end
    of the first statement. `sv` then dangles, and printing it is undefined
    behaviour. A `string_view` does not extend a temporary's lifetime.
  - Clang warns: `object backing the pointer will be destroyed at the end of
    the full-expression`. Fix: store the result in a `std::string`.
</details>

### Card M4-G9 · Spot the trap
Q: A fixed-width message `"121275"` holds a 2-character quantity, then a
   4-character price.
   ```cpp
   std::string msg = "121275";
   std::string_view qty = std::string_view(msg).substr(0, 2);  // "12"
   double q = atof(qty.data());
   ```
   The view is exactly `"12"`. What value does `q` get, why, and what should
   you call instead? Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `121275`: `atof` ignores the view's length and reads until the `'\0'` at
    the end of `msg` (`lab/atof_bug.cpp` prints this). With bytes that have
    no `'\0'` after them, it would read past the buffer: undefined
    behaviour.
  - Use `std::from_chars(qty.data(), qty.data() + qty.size(), value)`, which
    reads only the view's characters.
</details>

---

> Extra practice (optional): Card M4-PB3 in lesson-06-practice-bank.

---

## std::span and std::byte

### CONCEPT C12  std::span (C++20) and std::byte (C++17)

**Heartbleed: the length that lied.** On 7 April 2014 the popular OpenSSL
library shipped a fix for a bug that could "reveal up to 64k of memory to a
connected client or server". That limit was per heartbeat, and an attacker
could keep asking, pulling out "the secret keys used to identify the service
providers" along with user names and passwords.

The mechanism was a pointer and a length that nothing tied together. A
heartbeat message states how long its payload is. The code read that number
from the message and used it, never comparing it with the size of the record
that had actually arrived.

The fix added that comparison: if the claimed payload does not fit in the real
record, "silently discard".

Construct the span from the actual received buffer. Constructing it from the
untrusted claimed length merely repackages the bug. Validate the claimed
payload and required framing before accessing them; C++20 `span::operator[]`
is not a general bounds check.

`std::span` makes that pairing a type: the real length travels with the
pointer, so a length read off the wire always has `buf.size()` to be checked
against.

**`std::span<T>`** (header `<span>`) is `string_view`'s idea for any
element type: a pointer plus a length over contiguous elements it does not
own. A named vector, `std::array` or C array of matching elements normally
converts to it. Fine print: `vector<bool>` is not contiguous.
`span<const T>` gives read-only access.

Fine print: `char` and `unsigned char` buffers need an explicit byte view,
such as `std::as_bytes(std::span{buffer})`.

**`std::byte`** (header `<cstddef>`) is a scoped enum intended for raw bytes.
It supports bitwise operations, comparisons and assignment, but not ordinary
arithmetic such as addition. `std::byte{1} + std::byte{2}`
fails to compile (`invalid operands to binary expression`). Use
`std::to_integer<int>(b)` when you do need the value.

```cpp
// excerpt of lab/views_sv.cpp, plus the C++98 declaration for comparison
// C++98: pointer and length that nothing ties together
int sumBytes98(const unsigned char* data, std::size_t len);

// contiguous std::byte elements
int sumBytes(std::span<const std::byte> buf) {
    int total = 0;
    for (std::byte b : buf) total += std::to_integer<int>(b);
    return total;
}
std::vector<std::byte> rx(4, std::byte{0x01});
std::byte raw[3] = {std::byte{2}, std::byte{2}, std::byte{2}};
sumBytes(rx);    // 4
sumBytes(raw);   // 6
```

`lab/views_sv.cpp` prints `sum(vector)=4 sum(array)=6` and shows that both
`string_view` and `span<const byte>` are 16 bytes here: one pointer, one
length.

**The trap.** A span dangles exactly like a `string_view`: it must not
outlive the container it views.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| `span<const T>` | Length-carrying view; no ownership | Read a live contiguous buffer | Caller stores it beyond buffer lifetime |
| Pointer plus length | Two values must stay paired | Existing C boundary requires it | Pointer and count come from different buffers |
| `const vector<T>&` | Requires a vector object | API needs vector-specific behavior | C arrays need a copy just to call it |

Advice: take a span when the function only needs contiguous elements;
a length unrelated to its pointer is the smell in the old API.

**Rule:** take buffers as `std::span<const std::byte>` (or `span<const T>`),
and keep the owner alive.


---

### Card M4-C6 · Fix it
Q: Fill in both blanks so this contiguous `std::byte` buffer can be passed
   without copying its elements.
   ```cpp
// Fragment: fill the blanks in this exercise.
   void processPacket(std::___<const std::___> buffer) {
       // ... parse bytes ...
   }

   std::vector<std::byte> rx_buf(2048);
   processPacket(rx_buf);
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `span, byte`
  - The vector converts to `std::span<const std::byte>`: a pointer plus
    2048, no copy.
</details>

### Card M4-G8 · Spot the trap
Q: What is wrong with this function? Answer format: one or two sentences.
   ```cpp
   std::span<int> getBuffer() {
       std::vector<int> temp = {1, 2, 3, 4, 5};
       return temp;
   }
   ```
<details><summary>Answer</summary>
Key:

  - The span points into `temp`'s elements, which are freed when the
    function returns: the span dangles, and using it is undefined behaviour.
  - Clang warns (`address of stack memory associated with local variable
    'temp' returned`). Fix: return the `std::vector`.
</details>

---

## Two conveniences

### CONCEPT C13  CTAD (C++17): let the constructor pick the template arguments

**Why this exists.** Repeating an obvious type on both sides of a
constructor obscures the object being built. Deduction shortens that
case, but braced container construction still has overload rules worth
checking.

**Class template argument deduction** deduces a class template's arguments
from the constructor call, the way a function template call does.

```cpp
// two alternatives (my_mutex is a std::mutex); use one or the other
// C++11
std::lock_guard<std::mutex> lock(my_mutex);
std::pair<int, double> p(1, 2.0);

// C++17
std::lock_guard lock(my_mutex);   // std::lock_guard<std::mutex>
std::pair p{1, 2.0};              // std::pair<int, double>
```

**Braces with a vector: check the deduced type.**

| You write (`v2` is `std::vector<int>`) | Deduced type |
|---|---|
| `std::vector a{v2};` | `std::vector<int>` (a copy of `v2`) |
| `std::vector b{v2, v2};` | `std::vector<std::vector<int>>` |
| `std::vector c(v2.begin(), v2.end());` | `std::vector<int>` |
| `std::vector d{v2.begin(), v2.end()};` | a vector of two **iterators** |

With one argument, the copy wins. With several, braces prefer the
`std::initializer_list` constructor, so the arguments become the elements.

**Rule:** use CTAD for obvious cases like `lock_guard`; with braces and
containers, write the type or `static_assert` it.


---

### Card M4-Q10 · Apply
Q: (a) Shorten `std::lock_guard<std::mutex> lock(my_mutex);` using C++17
   CTAD. (b) With `std::vector<int> v2`, what types do
   `std::vector a{v2};` and `std::vector b{v2, v2};` deduce?
   Answer format: two short lines, (a) and (b).
<details><summary>Answer</summary>
Key:

  - (a) `std::lock_guard lock(my_mutex);`
  - (b) `a` is `std::vector<int>` (a copy); `b` is
    `std::vector<std::vector<int>>`, because with several arguments the
    braces pick the `initializer_list` constructor.
</details>

---

### CONCEPT C14  [[nodiscard]] (C++17; reason string C++20)

**Why this exists.** A caller can discard a value that was the whole point
of a function, such as a risk check. A compiler warning makes that
accident visible in review and builds.

`[[nodiscard]]` on a function asks the compiler to diagnose an ignored
result. The standard recommends a diagnostic rather than requiring one;
the tested Clang does warn. An explicit cast to `void` expresses intentional
discard. C++20 adds a reason: `[[nodiscard("...")]]`. libc++
(this toolchain's library) marks `std::vector::empty()` and `std::async`
this way (module 03, lesson 01); other libraries may differ.

```cpp
// fragment: orders is a std::vector<int>; the calls are inside a function
[[nodiscard]] bool withinRiskLimit(int qty);
orders.empty();          // meant clear(): warning
withinRiskLimit(5000);   // warning
(void)withinRiskLimit(1); // deliberate discard: no warning
```

Clang prints `ignoring return value of function declared with 'nodiscard'
attribute`, plus the reason text when there is one. It is a warning, not
an error, unless you build with `-Werror`.

**Rule:** mark functions whose result must be checked (risk checks, error
results) `[[nodiscard]]`, and build with warnings as errors where it
matters.


---


### Card M4-Q9 · Recall
Q: libc++ marks `std::vector::empty()` `[[nodiscard]]`. Why is that
   useful, and what does the attribute make the compiler do? Answer format:
   one or two sentences.
<details><summary>Answer</summary>
Key:

  - `empty()` only reports whether the vector is empty; a statement like
    `v.empty();` is almost certainly a mistake for `v.clear()`.
  - `[[nodiscard]]` asks the compiler to diagnose an ignored result; the
    tested Clang does warn. The standard recommends a diagnostic rather than
    requiring one. An explicit cast to `void` expresses intentional discard.
</details>

---

## Closing

Remember: a view (`string_view`, `span`) is a pointer and a length; keep
its owner alive and never treat it as a C string. CTAD with braces can
surprise you, so check the type. `[[nodiscard]]` asks for a diagnostic on
ignored results. Misses come back tomorrow. Next: lesson 05, where a throw
lands in the latency distribution (then module 05).
