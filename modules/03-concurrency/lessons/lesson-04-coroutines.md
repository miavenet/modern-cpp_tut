# Lesson 04 — Coroutines: Functions That Pause (C++20)

**Hook:** a function pauses halfway through a loop. What must survive until
it resumes? Commit your guess before opening the answer. **You will be able to:**
read and write a minimal C++20 generator, say what `promise_type`,
`coroutine_handle` and an awaiter do, and decide whether a coroutine belongs
on a hot path.

**Time:** about 39 minutes, including the module synthesis
cards. **You need:** lessons 01–03 and lambdas from module 01.

## Pretest — commit your guesses first

### Card M3-P4 · Pretest
Q: In C++98, how would you write a function that stops halfway, returns to
   its caller, and later resumes where it stopped? And which C++20 keywords
   do you think turn a function into a coroutine? Guess in one line.
<details><summary>Answer</summary>
Key:

  - C++98 has no language support: you write a state machine by hand (an
    object holding the locals plus a "where was I" state, resumed through a
    `switch`).
  - In C++20 a function is a coroutine if its body uses `co_await`,
    `co_yield` or `co_return`; the compiler builds the state machine.
</details>

### Card M3-P5 · Pretest
Q: A coroutine's local variables must survive while it is paused. Where do
   you think they are stored, and why might that worry a low-latency
   developer? Guess in one line.
<details><summary>Answer</summary>
Key:

  - In a *coroutine frame*. The standard says the implementation "may need
    to allocate" it, by calling an allocation function (the promise type's
    `operator new` if it declares one, otherwise the global one).
  - Compilers can sometimes elide that allocation, but are not required to.
    An allocation per call on a hot path adds latency you must measure or
    control.
</details>

---

## What a coroutine is

### CONCEPT C11  A coroutine you can read: a hand-written generator

Nishanov's 2018 defense separates two jobs: the compiler transforms
suspension into a state machine; the library decides what that machine means.
That is why this lesson defines `IntGen`, and why `co_await` alone does not
tell you which thread runs next. In the example below, identify who owns the
frame and who resumes it.

**Words first.** A *coroutine* (C++20) is a function that can *suspend*:
return control to whoever called or resumed it, keeping its locals, and
later *resume* from the same point. It is a coroutine if its body uses
`co_yield` (hand out a value, possibly suspending), `co_await` (wait for something,
suspending if needed) or `co_return` (finish).

A stackless coroutine preserves its own suspended state—such as its promise,
parameter copies and needed locals—without preserving an entire nested call stack.

**The C++98 way** was a class with the loop counter as a member and a
`next()` that does one step. The coroutine writes that class for you.

**The C++20 way: `lab/generator.cpp`.** The coroutine body is ordinary
code:

```cpp
// excerpt of lab/generator.cpp (IntGen is defined above it there)
IntGen count_to(int n) {
    for (int i = 1; i <= n; ++i)
        co_yield i;                 // suspend here, hand i out
}

int main() {
    IntGen g = count_to(3);         // body has not run yet
    while (g.next())                // resume until the next co_yield
        std::printf("got %d\n", g.value());
}
```

`IntGen` is about 30 lines in the lab file (its parts are the next
concept). Step by step:

1. `count_to(3)` creates the frame, then pauses before the first line.
2. `g.next()` resumes it. The loop runs to `co_yield 1`, stores 1, pauses.
3. `main` prints `got 1`. The same happens for 2 and 3.
4. The next resume leaves the loop and ends the body; `next()` returns
   false. `~IntGen` destroys the frame.

Output: `got 1`, `got 2`, `got 3`, then a `frames allocated:` line that
the third concept explains.

**C++23 adds `std::generator<T>`** (header `<generator>`), a standard
version of `IntGen`. **Toolchain note:** Apple clang 21 with libc++ does not
ship it: `#include <generator>` with `-std=c++23` stops with
`fatal error: 'generator' file not found`. Use the lab's `IntGen` here.

**The trap.** A coroutine cannot use a plain `return`. Clang says
`return statement not allowed in coroutine; did you mean 'co_return'?`.

**Rule:** a coroutine uses `co_return`, not ordinary `return`. Fine print:
falling off the end is valid only when its promise supports `return_void`;
otherwise it is undefined behavior.


---

### Card M3-C5 · Fix it
Q: With a C++23 library that provides `<generator>`, this produces 0..4
   lazily. Fill in the two blanks:
   ```cpp
// Fragment: fill the blanks in this exercise.
   std::_____<int> generate() {
       for (int i = 0; i < 5; ++i) {
           ____ i; // hand out i and suspend
       }
   }
   ```
   Answer format: one line, the two names in order, separated by a comma.
<details><summary>Answer</summary>
Key:

  - `generator, co_yield`
  - `std::generator` is C++23 and missing from this toolchain's libc++; the
    lab's `IntGen` plays the same role.
</details>

### Card M3-G9 · Spot the trap
Q: Using the lab's `IntGen`, a developer writes:
   ```cpp
   IntGen values(bool error) {
       if (error) return;   // stop early
       co_yield 1;
   }
   ```
   Why does this not compile, and what is the fix? Answer format: one or
   two sentences.
<details><summary>Answer</summary>
Key:

  - `co_yield` makes `values` a coroutine, and a coroutine may not contain a
    plain `return` statement (clang: `return statement not allowed in
    coroutine; did you mean 'co_return'?`).
  - Fix: write `co_return;`.
</details>

---

## The machinery

### CONCEPT C12  promise_type, coroutine_handle and awaiters

**Why these parts exist.** The language supplies the pause points, but a
generator and an asynchronous task need different rules for starting,
handing out values, and finishing. The return type's promise supplies
those rules; the handle and awaiter connect the paused frame to its caller.

**Three parts** (all C++20, header `<coroutine>`):

- **`promise_type`**: the compiler obtains it through `std::coroutine_traits`;
  normally this resolves to the return type's nested `promise_type`. Bare
  `co_return` calls `return_void()`; returning a non-void expression calls
  `return_value(expression)`. Fine print: `co_return` with a void expression
  evaluates it and then calls `return_void()`. `co_yield` passes its value to
  `yield_value` and awaits the result; whether it suspends depends on that awaiter.
- **`std::coroutine_handle<P>`**: a small, *non-owning* pointer to a frame,
  with `resume()`, `done()`, `destroy()` and `promise()`.
- **An awaiter**: what `co_await x` talks to. It has `await_ready()`,
  `await_suspend(handle)` and `await_resume()`. Usually `x` itself is the
  awaiter; a promise's `await_transform` or an `operator co_await` can turn
  it into a different one.

The lab's `IntGen::promise_type` (excerpt):

```cpp
std::suspend_always initial_suspend() noexcept { return {}; } // start paused
std::suspend_always yield_value(int v) noexcept {             // on co_yield v
    current = v;
    return {};                                                // pause again
}
```

`std::suspend_always` and `std::suspend_never` are the two ready-made
awaiters C++20 supplies. C++20 provides no ready-made task or generator
type, so you write the promise yourself or use a library.

**See it yourself: `lab/awaiter.cpp`.** `WaitForData` prints from each
member; `handler()` does `int v = co_await WaitForData{};`:

```
handler: before co_await
  await_ready -> false
  await_suspend: parking the handle
main: handler is suspended; resuming it
  await_resume -> 42
handler: resumed with 42
main: done
```

`await_ready` said "not ready", so the coroutine paused and
`await_suspend` stored its handle. Control went back to `main`, which later
called `parked.resume()`. `await_resume`'s return value became `v`.

**The trap.** A `coroutine_handle` does not own the frame. Destroy the
frame and every copy of the handle dangles.

**Rule:** the promise shapes the coroutine, the handle points at it, the
awaiter decides whether `co_await` pauses.


---

### Card M3-C4 · Fix it
Q: Name the three parts:
   (1) The _____ controls the coroutine from the inside, for example what
   happens on `co_yield` and when an exception escapes.
   (2) The _____ is the non-owning handle used to resume or destroy the
   paused frame.
   (3) An object is an _____ if it has `await_ready`, `await_suspend` and
   `await_resume`.
   Answer format: one line, three names in order, separated by commas.
<details><summary>Answer</summary>
Key:

  - `promise_type, coroutine_handle, awaiter`
  - "awaitable" is also accepted for (3): the `co_await` operand, which is
    usually the awaiter itself.
</details>

### Card M3-Q7 · Recall
Q: Why did a C++20 lazy sequence need about 30 lines of your own code, and
   what did C++23 add for this case? Answer format: one or two sentences.
Hint: Look at what `lab/generator.cpp` had to define.
<details><summary>Answer</summary>
Key:

  - C++20 gives the language rules plus low-level pieces
    (`coroutine_handle`, `suspend_always`, `suspend_never`) but no
    ready-made generator, so you write the return type and its
    `promise_type` yourself.
  - C++23 adds `std::generator<T>` in `<generator>` (not available in this
    toolchain's libc++).
</details>

### Card M3-G8 · Spot the trap
Q: Assume `MyTask` owns its coroutine frame and destroys it in its
   destructor. What is wrong?
   ```cpp
   std::coroutine_handle<> h;

   MyTask runTask() {
       co_await std::suspend_always{};
   }

   void setup() {
       auto task = runTask();
       h = task.get_handle();
   }   // task is destroyed here

   void later() {
       h.resume();
   }
   ```
   Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `~MyTask` destroys the frame at the end of `setup`, but `h` still points
    to it. `h.resume()` uses a destroyed frame: undefined behaviour (a
    use-after-free).
  - Fix: keep the owner (`task`) alive for as long as anyone may resume the
    handle.
</details>

---

## Coroutines and the hot path

### CONCEPT C13  The coroutine frame: allocated, elided, or yours

**Why you care.** A paused function needs storage for the state that a
normal stack frame would lose when it returned. That storage is a cost at
the call site, and a latency budget must account for it.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| Hand-written state object | You own its storage and transitions | A tiny fixed state machine is enough | State transitions are duplicated or obscure |
| Coroutine with measured elision | Readable suspended control flow; elision depends on build | The shipped build proves its allocation behavior | A debug-only measurement is treated as proof |
| Coroutine with a pool | Pool capacity and lifetime become your contract | Suspension is valuable and allocation must be controlled | Pool exhaustion has no policy |

Advice: use the coroutine when its pause points clarify the flow, then
count frame allocations in the build you ship; an unmeasured frame is the
smell on a hot path.

**The rule in the standard.** The implementation "may need to allocate"
storage for the frame. If the promise type declares an `operator new`, that
one is used; otherwise the global one. The compiler may also *elide* the
allocation (put the frame in the caller) when it can prove the frame does
not outlive the call and knows its size. Nothing guarantees elision.

**See it yourself.** `lab/generator.cpp` gives `promise_type` its own
`operator new` that counts calls. Build it twice:

| Build | Last line printed |
|---|---|
| `-O0` | `frames allocated: 1` |
| `-O2` (the lab command) | `frames allocated: 0` |

Same source; at `-O2`, Apple clang put this short-lived frame in `main`.
Change the program so the generator escapes (say, return it from a
function), and you may get the allocation back. Check your own case.

**Your options on a hot path.**
1. Keep coroutines on paths where an allocation is acceptable (start-up,
   connection handling, logging).
2. Give `promise_type` an `operator new` that takes memory from a
   pre-allocated pool, and prove the pool has room.
3. Rely on elision only if you check it in the optimised build you ship.

**The trap.** "Coroutines always allocate" and "the compiler removes the
allocation" are both wrong as rules. It depends on the code and the build.

**Rule:** on a hot path, count or control coroutine-frame allocations;
never assume them away.


---

### Card M3-J4 · Apply
Q: The rule for this service is "no heap allocation after start-up". An AI
   assistant's PR says `lab/generator.cpp` printed `frames allocated: 0` at
   `-O2`, so this generator is heap-free. Here the generator lives in a global
   `Session` and is read across ticks. Is the claim right for this code? What
   would you check first? Answer format: three short sentences.
   ```cpp
   // Fragment: IntGen and frames_allocated are from lab/generator.cpp.
   IntGen order_ids() { co_yield 1; co_yield 2; }
   struct Session { IntGen ids = order_ids(); };   // kept across ticks
   Session session;
   int main() {
       int sum = 0;
       while (session.ids.next()) sum += session.ids.value();
       std::printf("sum %d, frames allocated: %d\n", sum, frames_allocated);
   }
   ```
<details><summary>Answer</summary>
Key:

  - The claim does not carry over: the lab's `0` came from a short-lived
    generator inside `main`, and this one, built with the same `-O2`, prints
    `frames allocated: 1`.
  - I would first count frame allocations for the exact shape we ship, in the
    optimised build, with the promise's counting `operator new`.
  - That one allocation happens while the global is built, before `main`, so
    it may pass the start-up rule; any count that rises after trading starts
    would make me reject it.
</details>

---

### Card M3-Q8 · Apply
Q: You want a C++20 coroutine to parse the network stream inside the
   matching engine's hot loop, where no heap allocation is allowed. What
   must you check or provide before you adopt it? Answer format: one or two
   sentences.
<details><summary>Answer</summary>
Key:

  - Each call may allocate a coroutine frame through `operator new`, and
    elision is not guaranteed.
  - So either give `promise_type` an `operator new` that uses a
    pre-allocated pool, or count allocations in the optimised build you ship
    (as `lab/generator.cpp` does) and measure the latency; otherwise keep it
    off the hot path.
</details>

### Card M3-G7 · Spot the trap
Q: This runs once per incoming order on a latency-critical path. What hidden
   cost should you check? Answer format: one sentence.
   ```cpp
   MyTask processOrderAsync(Order order) {
       co_await validateOrder(order);
       co_await executeOrder(order);
   }
   ```
<details><summary>Answer</summary>
Key:

  - Each call may allocate a coroutine frame (it holds `order` and the
    suspension state) through `operator new`, unless the compiler elides it
    or `MyTask::promise_type` supplies its own allocator. Count it in the
    optimised build.
</details>

---

## Synthesis — the whole module

### Card M3-INT1 · Choose and explain
Q: A small trading pipeline has three threads: feed handler, strategy, and
   order gateway. For each coordination point, pick a tool from this module
   and give a one-line reason: (a) passing messages feed → strategy →
   gateway; (b) start-up, so no thread trades before all are ready;
   (c) shutdown; (d) a configuration reload that happens rarely.
   Answer format: four short lines, (a) to (d).
<details><summary>Answer</summary>
Key:

  - (a) Atomics with release/acquire publication between one producer and one
    consumer (module 06 builds such a queue); no blocking on the hot path.
  - (b) A `std::latch` counted down by each thread, waited on before
    trading starts; it is used once, off the hot path.
  - (c) `std::jthread` with `stop_token`: cooperative stop and automatic
    join, no detached threads.
  - (d) A `std::mutex` is fine where blocking is acceptable; keep it off the
    hot path.
</details>

### Card M3-INT2 · Choose and explain
Q: Post-mortem. A producer writes a plain (non-atomic) payload struct once,
   then sets `std::atomic<bool> ready` with `memory_order_relaxed`. The
   consumer spins on `ready.load(memory_order_relaxed)`, then reads the
   payload. The code is compiled separately for x86 and ARM; only the ARM
   build has been seen reading a stale payload.
   (a) Name the bug class. (b) Give the fix. (c) Why was it seen only on
   ARM? Answer format: three short lines, (a) to (c).
<details><summary>Answer</summary>
Key:

  - (a) Missing happens-before: relaxed creates no synchronizes-with link,
    so the payload write and read are a data race, undefined behaviour on
    both architectures.
  - (b) `ready.store(true, std::memory_order_release)` after writing the
    payload; spin on `ready.load(std::memory_order_acquire)` before reading
    it.
  - (c) x86 keeps stores in order with other stores, so the failure is rarely
    visible there; ARM may reorder independent accesses. Passing on x86 does
    not make the code correct, and ThreadSanitizer may not flag it either
    (lesson 02).
</details>

---

## Closing

Remember: any `co_` keyword makes a coroutine; the promise shapes it, the
handle does not own it, the awaiter decides whether `co_await` pauses.
Frame allocation may or may not happen, so count it. Misses come back
tomorrow. Next: lesson 05, spin or sleep measured at the tail (then module 04).
