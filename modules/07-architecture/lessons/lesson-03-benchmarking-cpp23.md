# Lesson 03 — Benchmarks You Can Trust, Plus C++23 Tools

Time: about 45 minutes (13 minutes reading, 10 minutes building and benchmarking, 9 cards). Optional dependency download and installation time are additional.

**The hook.** Two benchmarks of "add two numbers" can report similar
times even when one adds at run time and the other was folded by the
compiler. The timings alone cannot tell them apart; the assembly can.

**You will be able to:** build and run a Google Benchmark program (or a
library-free harness), stop the optimizer from deleting the work you time,
and use `std::flat_map`, `std::expected` and deducing `this` from C++23.

**You need from earlier:** module 06 lesson 03 (measure, do not guess).

## Pretest — guess first

### Card M7-P4 · Pretest
Q: You time a loop that adds integers 1,000,000 times, built with `-O2`, and
   never use the sum afterwards. The timer says 0 ns. Guess: what happened?
<details><summary>Answer</summary>
Key:
  - The result was never used, so the optimizer was allowed to delete the loop
    (dead-code elimination); the timer measured an empty region.
  - Fix: make the result observable (use it, or pass it to
    `benchmark::DoNotOptimize`), and check the assembly to see what is left
    inside the timed loop.
</details>

---

### CONCEPT C5  Keeping the work inside the timed loop

**The "do nothing" call that cost 81 microseconds.** In February 2022 a
Google Benchmark user posted two benchmarks that should have timed the same
nothing. One passed a million-int array to `DoNotOptimize`; the other passed
a pointer to it. Built with GCC 11, the array version took 81,655 ns per
iteration and the pointer version 0.242 ns; with Clang 13 both took about a
quarter of a nanosecond.

The assembly showed why: to satisfy the barrier's inline-assembly
constraint, GCC copied the whole array with `memcpy` on every iteration. A
maintainer's first reply was "Sounds like a GCC bug?" GCC's tracker did not
treat it as a bug, only as a missed optimisation, so the discussion turned
to changing the constraint inside Benchmark itself.

That report concerned Benchmark 1.6.1, not this course's pinned 1.9.5, and
the current guide shows another trap: `foo(0)` may become the constant 42
even when its result is kept. Look at the generated loop, including the
barrier, before you believe the timer.

**C++98 way.** `clock()` before and after a loop, compiled with `-O2`. The
optimizer may delete work whose result is unused, compute constant inputs at
compile time, or move work out of the timed region. You then time nothing.

**Google Benchmark** (v1.9.5, Apache 2.0) adjusts the iteration count to
meet its runtime budget and reports time per iteration. This does not establish
stability; use repetitions and inspect variability. Install it once, then build
`modules/07-architecture/lab/bench/bench_add.cpp` from the tutorial root:

```sh
./setup.sh --benchmark          # downloads and builds it into third_party/
clang++ -std=c++20 -O2 -I third_party/google-benchmark/include \
  modules/07-architecture/lab/bench/bench_add.cpp \
  -L third_party/google-benchmark/lib -lbenchmark -o "${TMPDIR:-/tmp}/bench_add"
"${TMPDIR:-/tmp}/bench_add"
```

The two "add" benchmarks in that file:

```cpp
static void BM_AddConstant(benchmark::State& state) {
    for (auto _ : state) {
        int r = add(2, 3);
        benchmark::DoNotOptimize(r);       // keeps r, not the work that made r
    }
}
static void BM_AddOpaque(benchmark::State& state) {
    int a = 2, b = 3;
    for (auto _ : state) {
        benchmark::DoNotOptimize(a);       // a and b become unknown to the compiler
        benchmark::DoNotOptimize(b);
        int r = add(a, b);
        benchmark::DoNotOptimize(r);
    }
}
```

The loop bodies at `-O2` (arm64) show the difference a similar timing can hide:

```
BM_AddConstant loop:  str w8, [sp, #12]        ; w8 holds 5, computed at compile time
BM_AddOpaque loop:    ldp w12, w11, [sp, #8]   ; load a and b
                      add w11, w12, w11        ; the real addition
                      str w11, [sp, #4]
```

**What the tools promise** (v1.9.5 user guide). `DoNotOptimize(x)` forces
the result `x` to be stored in memory or a register, but "does not prevent
optimizations on `<expr>` in any way": the expression that produced `x` can
still be folded. `ClobberMemory()` forces pending writes to global memory to happen;
memory owned by a local object must first "escape" through `DoNotOptimize`,
as `BM_VectorPushBack` in the same file does with `DoNotOptimize(v.data())`.
Warm-up "is set to 0 seconds and is therefore disabled" by default; enable it
with `MinWarmUpTime` if your experiment needs it.

**No library?** `lab/bench/mini_bench.cpp` uses `std::chrono::steady_clock`,
five repetitions and an empty `asm volatile` statement (the same trick as
`DoNotOptimize`, GCC and Clang only). Build:
`clang++ -std=c++20 -O2 modules/07-architecture/lab/bench/mini_bench.cpp -o "${TMPDIR:-/tmp}/mini_bench"`.
The library-free harness lets you inspect optimization without installing
Google Benchmark. Inspect its output and its generated loop together.

**Sub-nanosecond is not automatically wrong.** At 3 GHz one clock cycle
lasts about 0.33 ns. A small number tells you to look at the generated
loop, not that the benchmark is broken.

**Rule:** a timing means something only after you have seen, in the
assembly, that the work you meant to time is inside the loop.

| measurement | cost and guarantee | right when | wrong-choice smell |
|---|---|---|---|
| microbenchmark | isolates one operation; may miss queueing and optimizer changes | comparing local implementations | loop assembly lacks the intended work |
| production-like replay | includes interaction and load; harder to isolate a cause | checking end-to-end latency | input timing differs from the real feed |

Use a microbenchmark to explain a mechanism and a replay to decide whether
the system improved; a local win with a worse production tail is the smell.

### Card M7-J3 · Apply
Q: On the order path, each quote looks up a different symbol among 100,000,
   between bursts of other work. An AI assistant ran this C++20 benchmark
   and wrote "symbol lookup is negligible
   on the order path". Is that claim right here? What would you check first,
   and what would change your verdict? Answer format: three short sentences.
   ```cpp
   #include <chrono>
   #include <cstdio>
   #include <string>
   #include <unordered_map>
   int main() {
       std::unordered_map<std::string, int> ids;
       for (int i = 0; i < 100000; ++i) ids["SYM" + std::to_string(i)] = i;
       const std::string key = "SYM42";
       long sum = 0;
       auto t0 = std::chrono::steady_clock::now();
       for (int i = 0; i < 1000000; ++i) {
           sum += ids.find(key)->second;
           asm volatile("" : "+r"(sum) : : "memory");   // keep the work, like DoNotOptimize
       }
       auto t1 = std::chrono::steady_clock::now();
       std::printf("%.1f ns per lookup (sum %ld)\n",
           std::chrono::duration<double, std::nano>(t1 - t0).count() / 1e6, sum);
   }
   ```
<details><summary>Answer</summary>
Key:
  - Not for that claim: the benchmark honestly times a lookup, but always of the same key in a hot cache, which is not what the order path does.
  - First check the assembly to confirm that `find` runs inside the timed loop (here it does), then compare the benchmark's key pattern and cache state with production.
  - A varied-key benchmark or production-like replay must show a small cost under the relevant cache and load conditions before that claim stands.
</details>

### Card M7-C4 · Fix it
Q: Fill the blank so the compiler must keep `empty_string` as a real result:
   ```cpp
// Fragment: fill the blanks in this exercise.
   static void BM_StringCreation(benchmark::State& state) {
     for (auto _ : state) {
       std::string empty_string;
       benchmark::___(empty_string);
     }
   }
   ```
   Answer format: one word, the function name only.
<details><summary>Answer</summary>
Key:
  - `DoNotOptimize`: it forces `empty_string` to be treated as a used result,
    so its construction is not deleted as dead code.
</details>

### Card M7-Q7 · Recall
Q: (a) What does `benchmark::DoNotOptimize(x)` guarantee, and what does it
   not prevent? (b) What does `benchmark::ClobberMemory()` force?
   One sentence each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) It forces the value `x` to be materialised in memory or a register,
    so `x` is not removed as unused; it does not stop the compiler from
    optimising or constant-folding the expression that computed `x`.
  - (b) It forces all pending writes to global memory to actually be performed at
    that point (memory owned by a local object must first escape, for example
    through `DoNotOptimize(v.data())`).
</details>

### Card M7-G6 · Spot the trap
Q: Your Google Benchmark result is 0.2 ns per iteration with `-O2`.
   (a) Is that number alone proof the benchmark is broken? (b) Name the check
   that settles it and one change that makes compile-time-known inputs opaque.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) No. At 3 GHz one cycle is about 0.33 ns. A very small per-iteration
    time can be real throughput; inspect the assembly to see what ran.
  - (b) Read the assembly of the loop and confirm the work is there. If inputs
    are constants, pass them through `benchmark::DoNotOptimize` before using
    them, so the compiler cannot fold the computation.
</details>

---

> Extra practice (optional): Cards M7-PB30, M7-PB31 and M7-PB33 in lesson-06-practice-bank.

---

### CONCEPT C6  `std::flat_map` (C++23): a sorted map in two vectors

**Why another map?** A table built once but queried on every tick pays for
its lookup layout repeatedly. A sorted contiguous key array offers a
different trade: cheap scans of nearby keys, costly insertion shifts.

**C++98 way.** `std::map<std::string, Instrument>`. The standard specifies its
behaviour and logarithmic lookup, not its layout; libc++ implements it with a
node-based tree (`__tree`), each element in its own node.

**Modern way.** `std::flat_map` (C++23, header `<flat_map>`) is an adaptor
over two containers, by default `std::vector<Key>` and `std::vector<T>`, with
"the keys ... sorted". Lookup is a binary search over contiguous keys.
It needs a library that ships it (libc++ here does; libstdc++ 13 does not), so
check `__has_include(<flat_map>)`.
`lab/cpp23/flat_map.cpp` (build: `clang++ -std=c++23 ...`, command in the file):

```cpp
std::flat_map<std::string, double> px;
px["MSFT"] = 410.5;  px["AAPL"] = 227.1;  px["IBM"] = 210.0;
for (const auto& k : px.keys()) std::printf("%s ", k.c_str());   // AAPL IBM MSFT
for (double v : px.values()) std::printf("%.1f ", v);            // 227.1 210.0 410.5
```

**The cost.** The standard says inserting or erasing a single element is
linear: later elements in both vectors move. That suits a table built once at
startup and read many times.

**Python note.** A `dict` is not a sorted map: its keys must be hashable and
it iterates in insertion order (`{'MSFT':1, 'AAPL':2, 'IBM':3}` lists MSFT
first). The closer Python picture of `flat_map` is a sorted list of keys
searched with the `bisect` module.

**Rule:** reach for `flat_map` when lookups dominate and inserts are rare,
then measure against `std::map` on your real keys.

### Card M7-Q8 · Apply
Q: A `std::map<std::string, Instrument>` is filled once at startup and read on
   every tick. You consider C++23 `std::flat_map`. (a) In one sentence, why
   might lookups get faster? (b) In one sentence, what gets slower, and does
   it matter here?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `flat_map` keeps its keys sorted in contiguous storage, which can
    improve cache locality. A node-based map follows pointers between
    separately allocated nodes; measure on the real keys to confirm.
  - (b) Inserting or erasing one element is linear because later elements must
    move; here the map is filled once at startup, so it does not affect the
    per-tick path.
</details>

---

### CONCEPT C7  Explicit object parameters: one member body for several object types

**Why this tool appeared.** Repeating forwarding member overloads puts
ceremony around common operations. An explicit object parameter reuses one
member body.

**Deducing `this` (C++23).** A member function may name its object as an
explicit first parameter, `this Self&& self`. With a template parameter,
`Self` is deduced from the call expression. When called through a `Derived`
expression, the base-class template can deduce `Derived` without CRTP.
It needs a compiler that ships it: Apple clang 21 and clang 18 accept it, GCC 13
does not.
`lab/cpp23/deducing_this.cpp`
prints, for `Derived d; Base b; d.who(); b.who(); d.fixed();`:

```
who:   Self = Derived&
who:   Self = Base&
fixed: bound to Base&
```

`fixed` is declared `void fixed(this Base& self)`: a fixed type binds like any
reference parameter, including the normal derived-to-base conversion.

**`std::expected<T, E>` (C++23): a separate decision.** Routine validation
failure can be returned as an error value. The caller must inspect the result
before asking for its value. From `lab/cpp23/expected.cpp` (the full file
includes `<cmath>` and `<expected>` and declares `Err`):

```cpp
std::expected<long, Err> to_ticks(double px) {
    if (!std::isfinite(px) || px <= 0 || px > 1'000'000.0)
        return std::unexpected(Err::BadPrice);
    return static_cast<long>(px * 100);
}   // to_ticks(101.25).value() == 10125; to_ticks(-1).has_value() == false
```

This demonstration accepts prices up to 1,000,000 and truncates fractional
ticks. It is not an exact decimal-money parser. Calling `.value()` on an
error throws `std::bad_expected_access`.

**Rule:** A deduced `this Self&&` follows the call expression's type and value
category. Check an `expected` result before taking its value.
Fine print: through a `Base&`, it deduces `Base&` even when the object is a `Derived`.

### Card M7-Q9 · Apply
Q: `to_ticks(-1)` returns an error. A caller immediately uses `.value()` on
   a no-throw hot path. Is that safe? Show the decision the caller must make.
   Answer format: two short sentences.
<details><summary>Answer</summary>
Key:
  - No: `.value()` throws when the result contains an error. Test the result
    first; use the value on success and take the defined rejection path on
    failure.
</details>

### Card M7-G7 · Spot the trap
Q: `struct Base { template<class Self> void who(this Self&& self); void
   fixed(this Base& self); };` and `struct Derived : Base {};`. For a
   `Derived d;`: (a) what is `Self` in `d.who()`? (b) Does `d.fixed()` compile,
   and why?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `Self` is `Derived&`: the forwarding explicit object parameter deduces
    from the call expression `d`, an lvalue of type `Derived`.
  - (b) Yes. A fixed explicit object parameter `Base&` binds like any reference
    parameter, so the usual derived-to-base conversion applies.
</details>

### Card M7-INT2 · Choose and explain
Q: You are writing a C++23 order-matching component that uses `std::expected`
   for errors and `std::flat_map` for the book. In a short paragraph:
   (a) what evidence would you collect that it is correct, and (b) that it is
   fast? Say what each kind of evidence does not show.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Tests run under ASan plus UBSan, and separately under TSan if threads
    are involved, plus compiler warnings and review. A clean sanitizer run
    covers only the paths and timings the tests exercised; it is evidence, not
    proof.
  - (b) Benchmarks whose loops you have checked in the assembly (inputs made
    opaque, results kept with `DoNotOptimize`), compared against the previous
    `std::map` version on realistic keys, plus latency measured under
    realistic load. A micro-benchmark does not show behaviour under the real
    system's load.
</details>

---

> Extra practice (optional): Card M7-PB32 in lesson-06-practice-bank.

---

## Further reading

- [P2900R14: Contracts for C++](https://wg21.link/P2900R14) describes the
  C++26 runtime checks; compiler support remains toolchain-dependent.

## Closing

Remember: look at the assembly before believing a timing; `DoNotOptimize`
keeps a result, not the computation behind it; `flat_map` trades slow inserts
for contiguous lookups; a deduced `this` deduces from the call expression. Cards you missed
come back tomorrow. Next: lesson 04, CMake targets, or lesson 05, which reads
`flat_map` and `map` as latency distributions.
