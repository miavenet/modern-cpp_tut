# Lesson 04 — Parallel Algorithms, Literals and `source_location`

**Time: about 33 minutes** (reading about 15, cards about 18; running the samples is optional extra). Stop after any card; your place is saved.

**Hook:** A parallel count can lose to one thread when every worker increments the same atomic. Predict where the time goes before you measure it.

**You will be able to:** compile a parallel algorithm on this toolchain, avoid the shared-counter trap, write unit-safe durations, and log the caller's file and line without macros.

**Needs from earlier:** module 03's thread, mutex and atomic basics, plus lesson 02's cache-line idea.

## Pretests — commit your guesses FIRST

### Card M5-P4 · Pretest
Q: In C++98 you sort 10 million doubles with `std::sort(v.begin(), v.end())` on one thread. Guess: what one-argument change does C++17 offer to let the library sort in parallel?
<details><summary>Answer</summary>
Key:

  - Pass an execution policy first: `std::sort(std::execution::par, v.begin(), v.end());` with `#include <execution>`.
  - The policy *permits* parallel execution; it does not promise a number of threads or any speed-up. On this course's Apple clang it also needs the `-fexperimental-library` flag.
</details>

### Card M5-P5 · Pretest
Q: C++98 logging used macros such as `#define LOG(msg) log(msg, __FILE__, __LINE__)` to record the call site. Guess: what is awkward about the macro, and what might replace it?
<details><summary>Answer</summary>
Key:

  - A macro is text substitution: it ignores namespaces and scopes and cannot be passed around like a function.
  - C++20's `std::source_location::current()`, used as a default argument of an ordinary function, records the caller's file and line.
</details>

---

## Parallel algorithms

### CONCEPT C9  Execution policies, and making them compile here

**One `par`, very different engines.** In 2012 two camps brought the C++ committee separate plans for parallel algorithms: NVIDIA in one paper, Microsoft and Intel in another. The committee's study group SG1 suggested they merge, and the joint design parallelised the standard algorithms everyone already knew.

Then the implementations arrived, and they looked nothing alike. Microsoft's ran on Windows-specific tasking; Thibaut Lutz built his from plain `std::thread`; NVIDIA's was a thin wrapper around Thrust, a library that already existed.

The paper calls that variety deliberate: the abstractions "are intended to maximize flexibility of implementation". So typing `par` tells the library what it may do, not how many cores it must use.

**Words first.** C++17 lets many standard algorithms take an *execution policy* as a first argument (header `<execution>`). With `std::execution::seq`, element-function invocations stay in the calling thread and do not overlap; do not infer iterator order unless the algorithm specifies it. `par` may run on several threads; `par_unseq` may also interleave work within a thread for vector instructions. C++20 added `unseq`. `par` guarantees neither a thread count nor a speed-up.

**C++98 way vs modern way.**

```cpp
// fragment: before C++17 you split the range and started threads yourself
//           (std::thread is C++11; C++98 had no standard threads at all)
std::sort(v.begin(), v.end());                       // one thread
std::sort(std::execution::par, v.begin(), v.end());  // C++17: library may parallelise
```

**Toolchain note (Apple clang, libc++).** Plain `clang++ -std=c++20` rejects the policy: `error: no member named 'par' in namespace 'std::execution'`. Add `-fexperimental-library`:

```sh
clang++ -std=c++20 -O2 -fexperimental-library par.cpp && ./a.out
```

The link may print an `ld: warning` about the `libc++experimental.a` archive being built for a newer macOS; the program still ran correctly here. The archive's object name (`libdispatch.cpp.o`) shows which thread backend this library uses.

**See it yourself:**

```cpp
#include <algorithm>
#include <cstdio>
#include <execution>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

int main() {
    std::vector<double> v(10'000'000);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = double((i * 7919) % 1000003);
    std::sort(std::execution::par, v.begin(), v.end());
    std::printf("sorted: %s\n", std::is_sorted(v.begin(), v.end()) ? "yes" : "no");

    std::mutex m;                      // protects ids: shared state needs a lock
    std::set<std::thread::id> ids;
    std::for_each(std::execution::par, v.begin(), v.end(), [&](double) {
        std::lock_guard<std::mutex> lock(m);
        ids.insert(std::this_thread::get_id());
    });
    std::printf("for_each(par) ran on %zu thread(s)\n", ids.size());
}
```

<details><summary>What to check in the output</summary>

```text
sorted: yes
```

The second line reports how many threads this run used. One is a valid result.
</details>

**The wrong belief:** "parallel means faster and is fine anywhere." The library may allocate temporary memory (it throws `std::bad_alloc` if it cannot) and hand work to other threads, with no bound on how long that takes. That suits end-of-day batch jobs, not a latency-critical order path.

| Policy | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `seq` | Work stays in the calling thread | Small work or tight per-call latency | One core is saturated by a large batch |
| `par` | May use other threads and temporary storage; no speed promise | Large independent batch work | Tail latency or total time grows after the switch |

Benchmark both policies on the real batch size; keep `seq` when `par` loses or adds unacceptable delay.

**Rule:** use execution policies for big batch work, compile with `-fexperimental-library` on this toolchain, and measure before believing in a speed-up.

### Card M5-Q15 · Recall
Q: What does `std::execution::par` permit, and what does it leave up to the implementation? Answer in one sentence.
   Answer format: one line.
<details><summary>Answer</summary>
Key:

  - It permits concurrent execution, but the library may run sequentially and promises neither a thread count nor a speed-up.
</details>


### Card M5-C4 · Fix it
Q: Fill in the blanks to sort vector `v` in parallel, including the header. Answer on one line: the three blanks in order, separated by commas.
   ```cpp
// Fragment: fill the blanks in this exercise.
   #include <___>

   std::sort(std::___::___, v.begin(), v.end());
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - (1) `execution`
  - (2) `execution`
  - (3) `par` (`par_unseq` is also valid); on this toolchain the program then needs `-fexperimental-library`
</details>

### Card M5-Q10 · Apply
Q: A reviewer rejects `std::transform(std::execution::par, ...)` inside the order-matching loop of a latency-critical engine. In two sentences: what can the call do that the loop cannot afford, and where do such calls belong?
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - It may allocate temporary memory and hand the work to other threads (this library uses a libdispatch backend), and nothing bounds how long that takes, so each call adds unpredictable delay.
  - Parallel algorithms belong in batch work (end-of-day risk, sorting history), where total throughput matters more than the delay of any one call.
</details>

---

## Shared state in parallel loops

### CONCEPT C10  Data races, contended atomics, and reductions

**Why a reduction exists.** A parallel loop invites you to keep the old shared counter and make its increment atomic. That repairs correctness but funnels every worker through one cache line. Letting workers produce partial answers avoids that bottleneck and gives the library room to combine them.

**Words first.** With `par`, your function may run concurrently on several threads; the implementation may also execute it sequentially. If concurrent calls write the same `int` without synchronisation, that is a *data race*: undefined behaviour. An atomic fixes correctness but can make workers contend for one cache line (lesson 02). A *reduction* (`std::reduce`, `std::count_if`, `std::transform_reduce`) lets the library combine partial results however it likes.

```cpp
#include <algorithm>
#include <cstdio>
#include <execution>
#include <vector>

int main() {
    std::vector<int> orders(1'000'000, 1);
    int active_count = 0;
    std::for_each(std::execution::par, orders.begin(), orders.end(),
                  [&active_count](int o) { if (o) active_count++; });   // data race
    std::printf("racy count: %d (expected 1000000)\n", active_count);
}
```

The shared increment is undefined if callbacks overlap. Do not rely on any printed count. Measure sequential `count_if`, parallel increments of one atomic, and parallel `count_if` on the same input. Compare both elapsed time and the final count.

**The wrong belief:** "`memory_order_relaxed` makes the atomic cheap." Relaxed drops ordering, not the fight over the cache line.

| Counting method | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Shared plain `int` under `par` | Data race: undefined behaviour | Never for concurrent writes | A lambda captures the counter by reference |
| Shared atomic under `par` | Correct count; contended updates | Few updates across a large job | Every element increments the same atomic |
| `count_if` reduction | No user-owned shared counter; library combines results | Independent per-element test | More overhead than the sequential count on the actual input |

Use a reduction for independent counting, then compare its time with one thread.

**Rule:** avoid contended shared updates. Writes under `par` need race-free synchronization; for independent counting, prefer a reduction.


### Card M5-G7 · Spot the trap
Q: An end-of-day job must mark every order that breaches its limit. An AI assistant wrote this and commented "no shared writes, so no race". Is it right here? What would you check first, and what would change your verdict? (Fragment: `orders` is a `std::vector<Order>` whose `Order` has `qty`, `price`, `limit` and a `bool breach`; needs `<algorithm>` and `<execution>`.)
   ```cpp
   std::for_each(std::execution::par, orders.begin(), orders.end(),
       [](Order& o) { o.breach = o.qty * o.price > o.limit; });   // own element only
   ```
   Answer format: one line; separate (a), (b), and (c) with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) Yes for this batch: each call writes only its own element, and different elements of a `std::vector<Order>` are different memory locations, so the comment is true.
  - (b) First check that the lambda writes nothing except `o`: no captured counter, log or container.
  - (c) The verdict flips if the flags move into a `std::vector<bool>` written by index, which packs them into shared words, or if timing on the real batch shows `par` losing to one thread.
</details>

### Card M5-G12 · Spot the trap
Q: A colleague fixed a racy parallel counter with `std::atomic<long>` and `memory_order_relaxed`. It is now correct but slower than the old single-threaded loop. (a) Why? (b) What is the fix? One sentence each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) Every increment updates the same atomic, so concurrent workers contend for ownership of its cache line. Relaxed ordering does not remove that contention.
  - (b) Use a reduction such as `std::count_if` or `std::reduce` with the policy (the library may combine per-thread partial results; how is unspecified), then compare against one thread.
</details>

---

## Literals that carry meaning

### CONCEPT C11  User-defined literals and digit separators

**Why units belong in code.** A bare `50` can mean milliseconds in one function and microseconds in another. The mistake survives review because the integer looks innocent. A duration type makes the unit part of the argument the compiler sees.

**Words first.** A *literal* is a value written in code, like `50`. C++11 added *user-defined literals*: a suffix that calls a function, so `50ms` makes a `std::chrono::milliseconds`. The standard suffixes live in namespaces you must open: `std::chrono_literals` (`h`, `min`, `s`, `ms`, `us`, `ns`; C++14) and `std::string_literals` (`"abc"s` is a `std::string`; C++14). `using namespace std::literals;` opens both; `using namespace std::chrono;` also brings in the chrono ones.

**C++98 way vs modern way.**

```cpp
#include <chrono>
#include <cstdio>
using namespace std::chrono_literals;

// C++98: void sleepFor(int time);  is that ms or us? sleepFor(50) says nothing.
void sleepFor(std::chrono::milliseconds t) {
    std::printf("waiting %lld ms\n", (long long)t.count());
}

int main() {
    sleepFor(50ms);
    sleepFor(2s);      // seconds convert to milliseconds exactly
}
```

<details><summary>Real output, and the errors</summary>

```text
waiting 50 ms
waiting 2000 ms
```

`sleepFor(50)` does not compile (`no matching function for call to 'sleepFor'`): the constructor from a plain number is `explicit`. With `-std=c++11`, `50ms` fails with `invalid suffix 'ms' on integer constant`, because chrono literals are C++14. Without the `using`, `"hello"s` fails with `no matching literal operator for call to 'operator""s'`.
</details>

**Digit separators (C++14).** `'` may appear between digits: `1'000'000`. It changes readability, not the type. `int max_size = 1'000'000'000'000;` compiles with a warning here: `implicit conversion from 'long' to 'int' changes value from 1000000000000 to -727379968`. The literal is a `long` (64-bit here) and the conversion keeps only what fits in 32 bits. With braces, `int x{1'000'000'000'000};` is an error (narrowing).

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Bare integer | No unit encoded in the type | A local calculation with an obvious unit | Caller and callee disagree about ms versus us |
| Chrono literal and duration | Unit checked by the type system; conversion rules apply | Timing APIs and deadlines | A naked cast appears just to silence a unit mismatch |

Put units in the type at the boundary where a number becomes time.

**Rule:** put the unit in the type with a literal, open the literal namespace you need, and give big numbers a type big enough to hold them.


### Card M5-C5 · Fix it
Q: Fill in the blank so `100ms` compiles. Answer with only the namespace name that goes after `std::`.
   ```cpp
// Fragment: fill the blanks in this exercise.
   #include <chrono>
   using namespace std::___;
   auto timeout = 100ms;
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - `chrono_literals` (also valid: `literals`, `literals::chrono_literals`, or `chrono`)
</details>

### Card M5-G8 · Spot the trap
Q: This line is in a file that includes `<string>` and has no `using` directives. (a) Does it compile? (b) What one line fixes it?
   ```cpp
   std::string s = "hello"s + " world";
   ```
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) No: the `s` suffix is not visible, so clang reports `no matching literal operator for call to 'operator""s'`.
  - (b) `using namespace std::string_literals;` (or `using namespace std::literals;`) in the enclosing scope.
</details>

### Card M5-G9 · Spot the trap
Q: (a) What goes wrong here? (b) What type would you use instead? One line each.
   ```cpp
   int max_size = 1'000'000'000'000;
   ```
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) The value does not fit in this platform's 32-bit `int`, so the conversion loses the intended value. The digit separators are harmless; use a sufficiently wide type.
  - (b) A 64-bit type such as `std::int64_t` or `long long`.
</details>

---

## The caller's location without macros

### CONCEPT C12  `std::source_location` (C++20)

**Why a call site matters.** A logging function can know where its own body lives; it cannot infer where you called it from. The old macro passed file and line through text substitution. A defaulted `source_location` parameter lets an ordinary function carry that call-site information.

**Words first.** `std::source_location` (header `<source_location>`) is a small object holding a file name, line, column and function name. `std::source_location::current()` makes one. `current()` has special source-location behavior when used in a default argument: it captures the call that uses that default. Calling it inside the logger's body captures the logger instead.

**C++98 way vs modern way.** C++98: `#define LOG(msg) log(msg, __FILE__, __LINE__)`, a macro that ignores scopes. C++20:

```cpp
#include <iostream>
#include <source_location>
#include <string>

void logError(const std::string& msg,
              std::source_location loc = std::source_location::current()) {
    std::cout << loc.file_name() << ":" << loc.line() << " "
              << loc.function_name() << " - " << msg << '\n';
}

void submit() {
    logError("Order rejected");          // line 12: this line is reported
}

int main() { submit(); }
```

<details><summary>Real output (file saved as sl.cpp)</summary>

```text
sl.cpp:12 void submit() - Order rejected
```

Move `current()` into the body of `logError` instead and it reports a line inside `logError` every time.
</details>

**The wrong belief:** "`current()` always reports the caller." To capture the caller automatically, put `current()` in the parameter's default argument. A caller can also pass `current()` explicitly.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| `__FILE__`/`__LINE__` macro | Text substitution supplies the expansion site | Existing C++98 interface | Macro names leak through otherwise ordinary APIs |
| Defaulted `source_location` | Typed parameter captures the call site by default | A C++20 logging function | Every log reports the logger's own line |

Use the defaulted parameter for a new C++20 logger; a repeated logger line means `current()` was placed in the body.

**Rule:** take `std::source_location` as a defaulted last parameter to capture the caller.


### Card M5-Q9 · Recall
Q: (a) Why is `std::source_location` nicer than `__FILE__`/`__LINE__` macros? (b) Where should `std::source_location::current()` appear to capture the caller automatically? One sentence each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) It is an ordinary object and function parameter that respects scopes and types, instead of text pasted in by the preprocessor.
  - (b) As a default argument of the logging function's parameter, so its special rule reports the call site.
</details>

---

## Close

**Remember:** a policy permits parallelism, it does not promise speed; synchronize shared writes or use a reduction; open the literal namespace you use. **Tomorrow:** missed cards come back. **Next:** lesson 05, build a pool allocator with your own hands.
