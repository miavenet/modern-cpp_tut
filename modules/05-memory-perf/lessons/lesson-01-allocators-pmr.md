# Lesson 01 — Allocators and PMR: Controlling Where Data Lives

**Time: about 29 minutes** (reading about 14, cards about 15; running the samples is optional extra). Stop after any card; your place is saved.

**Module 05 adds** control over memory: where bytes come from (lessons 01 and 05), layout for the cache (02), compile-time work (03), parallel algorithms and literals (04). **It assumes** modules 01-02 and pointers, `new`/`delete`.

**Hook:** a `std::pmr::vector<int>` that grows to 100 elements asks for memory eight times on this course's library (capacities 1, 2, 4 ... 128). This lesson moves those eight requests into a 1 KB buffer you own, and shows the exact push that breaks the plan.

**You will:** back a container with your own buffer, predict when it falls back to the heap, and forbid that.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### Card M5-P1 · Pretest
Q: In C++98 you want a `std::vector<int>` to take its memory from one particular pool object instead of the heap. Guess in one sentence: what makes that hard, and what would you want C++ to change?
<details><summary>Answer</summary>
Key:

  - In C++98 the allocator is part of the container's type (`std::vector<int, PoolAlloc>` is a different type from `std::vector<int>`), and whether containers support allocators that carry state (such as "which pool") was implementation-defined until C++11.
  - C++17's PMR fixes both: every `std::pmr::vector<int>` has the same type, and you pass a pointer to the memory source when you construct it.
  - Any guess is fine; this card only primes the lesson.
</details>

---

## Arena allocation with PMR

### CONCEPT C1  The arena: a buffer you own, handed out front to back

**Two strings, same text, different types.** Picture a big system where most code is plain, non-template code, compiled once and linked many times. You want one function that takes a string, wherever its memory came from. With allocator template arguments you cannot write it: a string using `Alloc1` and a string using `Alloc2` are different C++ types, "even though they are both string types" holding the same values.

Pablo Halpern argued in 2005 that an object's type should not depend on its allocator. Fixing the allocator model "proved a time-consuming task", and this half missed C++11 entirely.

His 2013 proposal's own example: build short-lived shopping lists fast in a fixed buffer, then store them in a long-lived collection on the general heap, something "annoyingly difficult" without his fix. The fix was one allocator type holding a pointer to a `memory_resource`, so the memory source became a run-time value instead of part of the type. An arena is one resource you can put behind that pointer.

**Words first.** A *memory resource* is an object that hands out raw bytes: `allocate(bytes, alignment)` and `deallocate(...)`. `std::pmr::memory_resource` (C++17, header `<memory_resource>`) is the base class; `allocate` calls a virtual function `do_allocate` that each resource overrides. A `std::pmr::vector<T>` is just `std::vector<T, std::pmr::polymorphic_allocator<T>>`: an ordinary vector whose allocator holds a pointer to a resource. An *arena* (or bump allocator) hands out memory by moving a pointer forward through a buffer and does not reuse individual freed pieces. The standard arena is `std::pmr::monotonic_buffer_resource`.

**C++98 way vs modern way.**

```cpp
// fragment: C++98 needs a new allocator TYPE, and the container type changes
std::vector<int, PoolAlloc<int> > prices;      // not a std::vector<int>

// C++17: same container type for every memory source
std::pmr::vector<int> prices(&arena);          // arena is any memory_resource
```

**See it yourself** (compile with `clang++ -std=c++20 -Wall -Wextra arena.cpp && ./a.out`):

```cpp
#include <cstddef>
#include <cstdio>
#include <memory_resource>
#include <vector>

int main() {
    alignas(int) std::byte buffer[1024];                 // storage you own
    std::pmr::monotonic_buffer_resource arena(buffer, sizeof buffer);
    std::pmr::vector<int> prices(&arena);                 // uses the arena
    std::size_t last = 0;
    for (int i = 0; i < 100; ++i) {
        prices.push_back(i * 10);
        if (prices.capacity() != last) {
            last = prices.capacity();
            std::printf("capacity %zu\n", last);
        }
    }
}   // arena dies: its buffers are returned in one go
```

Before running it, **predict** how many capacity lines print.

<details><summary>Real output (Apple clang, libc++)</summary>

```text
capacity 1
capacity 2
capacity 4
capacity 8
capacity 16
capacity 32
capacity 64
capacity 128
```

Eight growths, so eight `allocate` calls, all served from `buffer`. Each time the vector grows it relocates elements into the new block and "frees" the old one, but the arena's `deallocate` does nothing: the old blocks stay used until the arena itself is destroyed or `release()` is called.
</details>

**The wrong belief:** "destroying the arena destroys my objects." It does not. It returns memory; destructors are the container's job. Freeing everything at once is cheap, but it is not free: objects are still destroyed one by one.

**Rule:** individual deallocations do not make arena space reusable. Fine print: after its users are finished, `release()` resets the arena for another batch; destruction also releases its upstream allocations.


### Card M5-C1 · Fix it
Q: Configure a PMR string to use the local arena when it needs dynamic storage. This short value can remain inside the string object through small-string optimization. Answer on one line: the three blanks in order, separated by commas.
   ```cpp
// Fragment: fill the blanks in this exercise.
   char buf[512];
   std::pmr::___ resource(buf, sizeof(buf));
   std::pmr::___ my_string("Fast string", &___);
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - (1) `monotonic_buffer_resource`
  - (2) `string`
  - (3) `resource` (the address of the resource object declared on the line above)
</details>

### Card M5-Q1 · Recall
Q: In one or two sentences: what does `monotonic_buffer_resource` do when a container gives memory back with `deallocate`, and when does that memory actually become free?
   Answer format: one line.
Hint: Think about the old blocks left behind each time the vector grew.

<details><summary>Answer</summary>
Key:

  - `deallocate` does nothing: individual freed pieces are not reused before `release()` or destruction.
  - The memory is given back all at once, when the resource is destroyed or `release()` is called.
</details>

### Card M5-Q2 · Apply
Q: Your message handler gives each message its own 1 KB stack arena, and an AI assistant's parser takes the resource as a parameter. Later the assistant added the `history.push_back` line so a dashboard can show past orders (headers `<memory_resource>` and `<vector>` assumed). Is the parser right here, and is the new line? What would you check first, and what would change your verdict? Answer in three short labeled parts.
   ```cpp
   std::pmr::vector<int> parseOrders(int n, std::pmr::memory_resource* mr) {
       std::pmr::vector<int> orders(mr);
       orders.reserve(n);
       for (int i = 0; i < n; ++i) orders.push_back(i);
       return orders;
   }
   std::vector<std::pmr::vector<int>> history;     // kept all session

   void onMessage(int n) {
       std::byte buf[1024];
       std::pmr::monotonic_buffer_resource arena(buf, sizeof buf);
       auto orders = parseOrders(n, &arena);        // used, then dropped here
       history.push_back(parseOrders(n, &arena));   // the new line
   }
   ```
   Answer format: one line; separate (a), (b), and (c) with semicolons.
Hint: Moving a `pmr::vector` keeps the same resource pointer.

<details><summary>Answer</summary>
Key:

  - (a) The parser is fine for `orders`, which dies before the arena, but the new line is wrong: the vector moved into `history` still points at the per-message arena and its buffer, which die when `onMessage` returns.
  - (b) First check, for every place a result is stored, whether that storage can outlive the resource it was built on (`get_allocator().resource()` tells you which resource a vector uses).
  - (c) It becomes right if `history` copies the values onto a long-lived resource, for example by constructing each history element with that resource.
</details>

---

## When the buffer runs out

### CONCEPT C2  Upstream, growth, and forbidding the fallback

**Why it matters.** A local buffer feels like a fence around heap use. It is only a first place to ask for bytes. If the next request does not fit, the default upstream can quietly cross that fence; a latency budget deserves an explicit overflow policy.

**Words first.** When the arena's buffer has no room for a request, it asks its *upstream* resource for another buffer. If you do not name an upstream, it uses `std::pmr::get_default_resource()`, which is normally the ordinary heap (`new`/`delete`). So "I gave it a buffer" does **not** mean "it never touches the heap." An arena with no buffer asks upstream for its first allocation.

**Why 100 ints fit, step by step.** On the shown library, those capacities use 4 + 8 + 16 + 32 + 64 + 128 + 256 + 512 = **1020 bytes** of the 1024-byte buffer, because the old blocks are never reused. Push number 129 needs a 256-int block (1024 bytes); only 4 bytes are left, so the arena must ask upstream.

The 1024-byte buffer was a lucky fit for this growth sequence (it doubles; libstdc++ 13 gave the same capacities and the same spill at push 129). Another library, or another element type, can spill earlier.

**Two fixes.** If you know the final size, reserve it: one block, no growth waste. If this arena must never fall back upstream, use `std::pmr::null_memory_resource()`, whose `allocate` always throws `std::bad_alloc`:

```cpp
#include <cstddef>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <vector>

int main() {
    alignas(int) std::byte buffer[1024];
    std::pmr::monotonic_buffer_resource arena(buffer, sizeof buffer,
                                              std::pmr::null_memory_resource());
    std::pmr::vector<int> prices(&arena);
    try {
        for (int i = 0; i < 129; ++i) prices.push_back(i);
    } catch (const std::bad_alloc&) {
        std::printf("bad_alloc after %zu ints\n", prices.size());
    }
}
```

<details><summary>Real output</summary>

```text
bad_alloc after 128 ints
```

With `prices.reserve(250)` before the loop, 250 ints (1000 bytes) fit with no exception.
</details>

**Alignment of the buffer.** Each type needs its address to be a multiple of its *alignment* (`alignof(double)` is 8 here). The arena rounds each request up to the alignment asked for, so a plain `char` buffer still works; it may need padding. `alignas(int)` above avoids initial padding for an `int`.

**One cost to know.** Every PMR allocation is a call through a `memory_resource*` to the virtual `do_allocate`. `std::allocator` has no such virtual step. Whether that matters is a measurement question.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Ordinary `new`/`delete` | General allocation; each object can be freed separately | Long or mixed lifetimes | Heap calls appear in a path meant to avoid them |
| Monotonic arena | Cheap bump requests; released pieces wait for `release()` or destruction | Everything dies together | Memory held rises through repeated create/cancel cycles |
| Pool resource | Reuses freed blocks; may fetch chunks upstream | Repeated creation and deletion | The upstream counter keeps rising at a steady peak |

Choose by lifetime first; if the counter or retained memory disagrees, revisit the choice.

**Rule:** a buffer is a capacity plan; reserve for known sizes and use `null_memory_resource()` when arena fallback must be an error.


### Card M5-G1 · Spot the trap
Q: This loop runs on the hot path (fragment: inside a function, with `<memory_resource>` and `<vector>`). Where does the memory come from? Answer in one or two sentences.
   ```cpp
   std::pmr::monotonic_buffer_resource arena;   // no buffer, no upstream named
   std::pmr::vector<int> v(&arena);
   for (int i = 0; i < 10000; ++i) v.push_back(i);
   ```
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - Every block comes from the upstream, which defaults to `get_default_resource()` (normally new/delete).
  - Fix: give it a buffer sized for the workload, `reserve`, and pass `null_memory_resource()` if arena fallback must be forbidden.
</details>

### Card M5-G2 · Spot the trap
Q: Your micro-benchmark shows `std::pmr::vector` slightly slower than `std::vector` for single `push_back`s, even though the arena never runs out. Name the one extra step every PMR allocation takes (one sentence).
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - A call through a `memory_resource*` to the virtual `do_allocate`; `std::allocator` has no virtual call. Measure whether it matters for your workload.
</details>

### Card M5-INT1 · Choose and explain
Q: A message parser builds a `std::vector<Field>` of up to 32 fields for every message, then throws it away. In two or three sentences: how would you rewrite it with an arena, and how would you check whether the whole parser still uses the heap?
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - Give each message a local buffer and a `monotonic_buffer_resource` over it; build a `std::pmr::vector<Field>` on it and `reserve(32)`.
  - Choose the buffer size from the worst case, not from luck with the growth sequence.
  - Use a null upstream to make arena overflow fail, or a counting upstream to observe fallback requests. Then inspect or instrument allocations inside `Field` and other called code to check the whole parser.
</details>

---

## Building objects in raw bytes yourself

### CONCEPT C3  Placement `new`, alignment, and `std::launder`

**Why it matters.** A container hides object construction. Once you write the pool yourself, raw bytes and live objects are separate things; mixing them up can make a fast path undefined even when the addresses look right.

**Words first.** *Placement new* (`new (p) T{...}`, header `<new>`) builds an object in storage you already have and returns a pointer to it. PMR containers do this for you; a hand-written arena or pool makes you do it.

**Trap 1: alignment.** `alignof(std::byte)` is 1, so a `std::byte` array may start at any address. Building a `Tick` (alignment 8 here) in it needs `alignas(Tick)` on the array; a misaligned object is undefined behaviour.

**Trap 2: the buffer pointer is not the object pointer.**

```cpp
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>

struct Tick { const std::uint32_t seq; double px; };   // note the const member

int main() {
    alignas(Tick) std::byte slot[sizeof(Tick)];
    Tick* t1 = new (slot) Tick{1, 100.25};
    t1->~Tick();
    Tick* t2 = new (slot) Tick{2, 101.50};               // slot reused
    std::printf("%u\n", t2->seq);                        // OK: placement new's pointer
    std::printf("%u\n", std::launder(reinterpret_cast<Tick*>(slot))->seq);   // OK
    std::printf("%u\n", t1->seq);    // OK in C++20; undefined in C++17 (const member)
    // reinterpret_cast<Tick*>(slot)->seq without launder: undefined in every standard
}
```

This prints `2` three times here. `reinterpret_cast<Tick*>(slot)` still points at the byte array, not at the `Tick` inside it, so using it is undefined behaviour. `std::launder` (C++17) turns it into a pointer to the object that now lives there.

**The C++17 vs C++20 detail.** In C++17, an old pointer such as `t1` did not refer to the new object when the class had a `const` or reference member. C++20 dropped that restriction, so `t1->seq` is valid in C++20 here.

**Rule:** keep the pointer placement `new` returns; use `std::launder` only when you must rebuild a pointer from raw storage.


### Card M5-G10 · Spot the trap
Q: Your order pool reuses slots. A reviewer says the last line is undefined behaviour even though tests pass. Answer (a) why, in one sentence, and (b) the fix, as one line of code.
   ```cpp
   // Fragment inside a function; Order and process are declared.
   alignas(Order) std::byte storage[sizeof(Order)];
   std::byte* slot = storage;
   Order* o = reinterpret_cast<Order*>(slot);
   new (slot) Order{next_id, px};
   process(o->id);
   ```
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) In this example, `o` comes from the byte array’s pointer, not from an `Order` pointer; keep the pointer returned by placement `new` before accessing the object.
  - (b) `Order* o = new (slot) Order{next_id, px};` (keep placement new's result), or `Order* o = std::launder(reinterpret_cast<Order*>(slot));` after the placement new.
</details>

### Card M5-Q13 · Recall
Q: When you reuse a byte buffer for another object, which pointer is safest to keep, and what must be true of the buffer? Answer in one sentence.
   Answer format: one line.
<details><summary>Answer</summary>
Key:

  - Keep the pointer returned by placement `new`, and make the buffer large enough and aligned for the object.
</details>

---

> Extra practice (optional): Card M5-PB27 in lesson-07-practice-bank.

---

## Close

**Remember:** an arena reuses space after `release()`, not after individual deallocations; a buffer is a plan, so reserve and use `null_memory_resource()` to forbid upstream fallback; keep placement new's pointer. **Tomorrow:** missed cards come back. **Next:** lesson 02, cache lines and data layout.
