# Lesson 06 — Practice Bank (optional): Clocks and Passing by Value

**Time: about 80 minutes** for 11 cards (8 + 12 + 5 + 5 + 10 + 6 + 4 + 8 + 12 + 6 + 4 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a function that takes 15 ns, and a per-call log that says it took 0 ns most of the time.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M1-PB1 · Apply
Q: A teammate says: "`steady_clock` counts in nanoseconds, so I will time every call of our short pricing function with two `now()` calls and log each result." The function takes a few tens of ns at most. List two things that make the logged numbers wrong, and the measurement that exposes each.
   Answer format: two short points, each with the measurement that exposes it.
<details><summary>Answer</summary>
Key:

  - **Resolution.** Here the clock advances in 41.67 ns ticks. A function shorter than a tick mostly reads as 0. Measured with a function that takes about 15 ns per call: over 200,000 single-shot timings, more than half read 0 ns in every run (the exact share changed from run to run with machine load, so it is not given here) and almost all the rest read 41 or 42 ns. No sample is close to the true cost. Expose it: print a histogram of the single-shot values.
  - **Read overhead.** Each `now()` costs about 20 ns here (the program's `wall/read` line), more than the function itself. Expose it: time a batch of back-to-back empty `now()` calls and divide by the count (a single empty pair also mostly reads 0 here). The `wall/read` loop also converts and stores each value, so it slightly overstates the bare read.
  - Remedy for both: time a batch. One measurement around 20 million calls gave about 15 ns per call in every run (one machine). Compare the single-shot histogram with the batched mean; if they disagree, trust the batch for cost.
  - Accept other valid points if each comes with a measurement that exposes it.
  - Holds for / re-check: the tick size and the 15 ns function are this Mac's; the method (histogram of single shots, compare with a batch) works for any clock and any short function. Re-check the tick on each new Mac: `clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/clock_res.cpp -o "${TMPDIR:-/tmp}/clock_res" && "${TMPDIR:-/tmp}/clock_res"` prints the single-shot values and the batched cost. `clock_res.cpp` builds on macOS only (it includes `<mach/mach_time.h>`); on another OS, port its `mach_timebase_info` lines to that OS's clock API, or apply the same method (single-shot histogram, then a batch) with your own timer.
</details>

### Card M1-PB2 · Apply
Q: An 8-byte handle `struct P { long h; };` is passed by value to a function in another file. Four variants add one thing each: `D` a user-written destructor `~D();`, `DF` a destructor `= default` in the class, `OD` a copy constructor declared in the class and defaulted out of line (`OD::OD(const OD&) = default;`), `DL` deleted copy and move constructors. Predict which variants travel in a register and which are stored to the caller's stack and passed by address; for `DL`, say what you expect and why. Then compile for arm64 and x86-64 and read the call sites:
   ```sh
   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=arm64-apple-macos
   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=x86_64-linux-gnu
   ```
   Answer format: list the register variants and the stack variants, say where you expect `DL` and why, and give the rule in one line.
<details><summary>Answer</summary>
Key:

  - Register, plain tail jump: `P` and `DF` (arm64 `b __Z5sinkP1P`; x86-64 `jmp _Z5sinkP1P@PLT`).
  - Stack and address: `D` and `OD`. arm64, both: `str x0, [sp, #8]` / `add x0, sp, #8`. x86-64 `D`: `movq %rdi, 8(%rsp)` / `leaq 8(%rsp), %rdi`; x86-64 `OD`: `movq %rdi, (%rsp)` / `movq %rsp, %rdi`. For `D` the caller also calls `~D` after the call returns.
  - Reason: under the Itanium C++ ABI a type with a non-trivial copy constructor, move constructor or destructor is "non-trivial for the purposes of calls", and "the caller must allocate space for a temporary and pass that temporary by reference". The C++ standard permits the extra temporary that makes the register path possible only "if X has at least one eligible copy or move constructor (11.4.4), each such constructor is trivial, and the destructor of X is either trivial or deleted" ([class.temporary]/3). A deleted function is not eligible ([special]/6).
  - Why `OD` is non-trivial although it says `= default`: a function is user-provided if it is "not explicitly defaulted or deleted on its first declaration" ([dcl.fct.def.default]/5), and a user-provided copy constructor or destructor is not trivial.
  - `DL`: either placement is a hit if you give the rule behind it (stack: Itanium treats a type whose copy and move constructors are all deleted as non-trivial for calls; register: no non-trivial copy, move or destructor). This clang passed it in a register; see the Holds-for line.
  - Rule: to keep a small type in registers, remove the user-written copy constructor, move constructor and destructor, or `= default` them on their first declaration.
  - Holds for / re-check: Itanium-ABI targets (Linux, macOS), Apple clang 21, x86-64 compile-only. `DL` is an observed disagreement: it has no eligible copy or move constructor, so [class.temporary]/3 does not grant the permission, and Itanium 1.1 calls a type whose copy and move constructors are all deleted non-trivial for calls; yet this clang passed `DL` in a register on both targets (`b __Z6sinkDL2DL`, `jmp _Z6sinkDL2DL@PLT`). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 (arm64) and clang 18 (arm64 and x86-64) passed `DL` by address, so the register placement is specific to this compiler version. Do not rely on either for such types; check your compiler. Other ABIs (e.g. Windows) were not checked, and the cost of the memory trip was not measured. Re-check by compiling the same file with your compiler and target.
</details>

### Card M1-PB3 · Apply
Q: `Owner` is a move-only owning handle whose destructor prints a line. `take(Owner)` takes it by value. `main` runs one full-expression: `(take(Owner(new int(1))), std::puts("rest of the same full-expression"))`. Predict where "~Owner" prints, with and without `[[clang::trivial_abi]]` on `Owner`. Then build and run both:
   ```sh
   clang++ -std=c++20 -O2 modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/plain" && "${TMPDIR:-/tmp}/plain"
   clang++ -std=c++20 -O2 -DUSE_TRIVIAL_ABI modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/tabi" && "${TMPDIR:-/tmp}/tabi"
   ```
   Answer format: one line for each build.
<details><summary>Answer</summary>
Key:

  - Without the attribute (this compiler): the parameter is destroyed by the caller, after the rest of the full-expression.
  - With it: the callee destroys the parameter before returning, so "~Owner" prints before "rest of the same full-expression".
  - The clang docs also warn the compiler "may omit the call to the copy constructor", so its side effects may not happen; types that depend on their own address "should not be declared [[trivial_abi]]".
  - Holds for / re-check: clang only (a `clang::` attribute). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 ignored it with a warning (`scoped attribute directive ignored`) and printed "~Owner" after "rest of the same full-expression" in both builds. When a by-value parameter is destroyed without the attribute is implementation-defined in C++ ([expr.call]), so check your compiler. Re-check the clang attribute docs for the current rules.
</details>

### Card M1-PB4 · Recall
Q: On Linux you need two clocks: one to measure how long a request took (it must never jump if an admin changes the time), and one to print the time of day in a log. For each clock, say which job it fits and why: `CLOCK_REALTIME`, `CLOCK_MONOTONIC`, `CLOCK_MONOTONIC_RAW`. First attempt: look it up in the Linux `clock_gettime(2)` man page (man7.org); on reviews, recall it.
   Answer format: one line per clock.
<details><summary>Answer</summary>
Key:

  - `CLOCK_REALTIME`: time of day. It is affected by discontinuous jumps (the time being set) and by NTP frequency adjustments. Bad for intervals.
  - `CLOCK_MONOTONIC`: intervals. Not affected by jumps, but still affected by frequency adjustments.
  - `CLOCK_MONOTONIC_RAW`: intervals at the raw hardware rate; not subject to frequency adjustments.
  - Holds for / re-check: Linux, per the `clock_gettime(2)` man page. macOS has the same three names plus others (e.g. `CLOCK_UPTIME_RAW`), with different rules: its `CLOCK_MONOTONIC` keeps counting during sleep (`man 3 clock_gettime`). Read that platform's man page before reusing the choice.
</details>

### Card M1-PB5 · Apply
Q: The "small buffer" is space inside a `std::function` object where a small callable is stored without a heap allocation. Find its usable size on your library yourself: store lambdas that capture `std::array<char, N>` for N = 8, 16, 24, 32, 40, 64 and count the heap allocations the construction makes. Also print `sizeof(std::function<int(int)>)`, and test a 16-byte capture with `alignas(16)` and three 8-byte captures that differ only in a user-provided special member (throwing move, `noexcept` move, throwing copy with `noexcept` move). Predict first, then run (and with `g++` on Linux):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/function_sbo.cpp -o "${TMPDIR:-/tmp}/function_sbo" && "${TMPDIR:-/tmp}/function_sbo"
   ```
   Answer format: `sizeof`, the largest capture with 0 allocations, then one line each for the alignment and special-member probes.
<details><summary>Answer</summary>
Key:

  - Both libraries tried (libc++ and libstdc++): `sizeof` is 32; captures of 8 and 16 bytes cost 0 allocations; 24 bytes and more cost 1.
  - So the usable inline space is smaller than the object (16 of 32 bytes here). Do not guess it from `sizeof`.
  - Size is not the only condition. A 16-byte capture with `alignof` 16 allocated once on all three toolchains tried.
  - 8-byte captures with a user-provided special member: libc++ allocated 0 for a throwing move and 0 for a `noexcept` move, but 1 for a throwing copy (with `noexcept` move). libstdc++ (GCC 13.3 and clang 18.1.3) allocated 1 for all three (none of these captures is trivially copyable). So a throwing move is not the cause on libstdc++: a user-provided `noexcept` move also allocates there.
  - So the rules differ per library and are not the same set: libc++ here was driven by a nothrow copy; libstdc++ here by the capture being trivially copyable (the plain 8-byte `char` array was). Do not assume a rule from one library.
  - The method transfers: replace `operator new`, count, bisect the size. Use it for any type-erased wrapper (`std::any`, a callback class of your own).
  - Holds for / re-check: libc++ (Apple clang 21) and libstdc++ 13 on 64-bit arm64 (GCC 13.3 and clang 18.1.3 on Ubuntu 24.04 in a container). Other versions, other ABIs, or MSVC can differ, and the standard sets no size. Only N = 8..64 were tested, and the libstdc++ trivially-copyable explanation is an inference from the three probes, so the threshold is known only to lie between 16 and 24 bytes for these captures. Re-run the probe after a toolchain change.
</details>

### Card M1-PB6 · Spot the trap
Q: An event bus keeps `std::vector<std::function<int(int)>> handlers`. A reviewer says "handlers are cheap to pass around, `std::function` never allocates after construction". The handler captures 32 bytes of state. The vector has spare capacity (`reserve(8)`). Which of these allocate: (1) `push_back` of a copy of the handler, (2) `push_back` of a moved handler, (3) `reserve(64)` reallocating a vector that already holds 4 handlers (how many allocations: the new vector buffer, per-handler copies, or both)? Run this, and the `large target` and `move ctor noexcept` lines of card M1-PB5's program:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/function_vec.cpp -o "${TMPDIR:-/tmp}/function_vec" && "${TMPDIR:-/tmp}/function_vec"
   ```
   Answer format: an allocation count for (1), (2) and (3), then the fix.
<details><summary>Answer</summary>
Key:

  - (1) Copy into reserved space: 1 allocation (the copied heap-stored target; none for the vector buffer because capacity was reserved).
  - (2) Move into reserved space: 0 allocations. The move steals the pointer.
  - (3) Reallocating 4 elements: 1 allocation in total, the new vector buffer, and 0 per-element target allocations, not 5. `std::function`'s move constructor is `noexcept` (the probe prints 1), so the vector moves its elements instead of copying them. (Without reserved capacity, each growth step still allocates a new vector buffer.)
  - **Fix:** `push_back(std::move(h))` or `emplace_back` with the lambda; pass by `const&` or move, not by value-then-copy.
  - Holds for / re-check: measured on libc++ (Apple clang 21) and libstdc++ 13. The `noexcept` on the move constructor is required by the C++20 and C++23 drafts ([func.wrap.func.con]), so (3) holds for any conforming library; (1) and (2) depend on the target being too big for the small buffer. Re-check by running both programs.
</details>

### Card M1-PB7 · Recall
Q: Where in the C++ standard would you look to find out whether `std::function` must avoid the heap for small callables, and what does it say? What does that mean for code that must never allocate on the hot path?
   Answer format: the section, what it says, then the rule for hot-path code.
<details><summary>Answer</summary>
Key:

  - [func.wrap.func.con] only has "Recommended practice: Implementations should avoid the use of dynamically allocated memory for small callable objects", with the example of a target holding only a pointer or reference to an object and a member function pointer.
  - "Recommended practice" is not a requirement. Copy and converting construction "may throw bad_alloc" unless the target is a `reference_wrapper` or a function pointer.
  - So the only guarantee is a no-throw guarantee ("Throws: Nothing"), and so in practice no allocation, for function pointers and `std::reference_wrapper` targets. For anything else, measure on your library (card M1-PB5), or use a wrapper with a fixed, documented inline size.
  - Holds for / re-check: wording checked in the C++23 draft N4950. Older drafts (C++11 to C++17) word it as a note ("encouraged to avoid"). Re-check the current draft for the newer wrappers (`move_only_function`, `function_ref`).
</details>

### Card M1-PB8 · Apply
Q: Paste `modules/01-foundations/lab/practice/lambda_closure.cpp` (a lambda `[step, &total](int x) { total += step * x; }`) into C++ Insights (cppinsights.io, C++20). What class does the compiler generate? Then predict, and check with the second program: (a) `sizeof` the lambda on a 64-bit target; (b) `total` after creating the lambda with `step = 3`, calling `apply(4)`, setting `step = 9` and calling `apply(2)`; (c) whether `[step]() { ++step; }` compiles (the `-DBUMP` build).
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/lambda_snapshot.cpp -o "${TMPDIR:-/tmp}/lambda_snapshot" && "${TMPDIR:-/tmp}/lambda_snapshot"
   clang++ -std=c++20 -O2 -Wall -Wextra -DBUMP -fsyntax-only modules/01-foundations/lab/practice/lambda_snapshot.cpp
   ```
   Answer format: the class members and call operator, then (a), (b), (c).
<details><summary>Answer</summary>
Key:

  - C++ Insights shows a class with members `int step;` and `int & total;` and an `inline constexpr void operator()(int x) const`.
  - (a) `sizeof(apply)` = 16 here (an `int` plus a reference stored as a pointer, with padding).
  - (b) A by-value capture is a snapshot taken when the closure is created: both calls use its stored `step = 3`, so `total=18` (3*4 + 3*2).
  - (c) Error: "cannot assign to a variable captured by copy in a non-mutable lambda" (because `operator()` is `const`; `mutable` removes that).
  - Technique: when you are unsure what a language feature becomes, look at the lowered C++ with C++ Insights before reasoning about speed.
  - Holds for / re-check: Apple clang 21 arm64 for (a) to (c). Closure layout is implementation-specific, so re-check `sizeof` per target. C++ Insights output is clang-based.
</details>

### Card M1-PB9 · Apply
Q: `vector_grow.cpp` builds a `std::vector<T, CountingAlloc<T>>` and makes 1000 `emplace_back` calls. It prints every new capacity, the number of allocations, and how many times `T` was copy- or move-constructed. It runs three ways: (a) `T` whose move constructor is *not* `noexcept`; (b) the same type with a `noexcept` move constructor; (c) type (b) with `reserve(1000)` first. Predict all three counts first, then run it:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/vector_grow.cpp -o "${TMPDIR:-/tmp}/vector_grow" && "${TMPDIR:-/tmp}/vector_grow"
   ```
   Answer format: capacities and allocations, copies and moves for (a), (b), (c), then why (a) copies and why (c) works.
<details><summary>Answer</summary>
Key:

  - (a) and (b): capacities 1 2 4 8 16 32 64 128 256 512 1024, so 11 allocations. Same on libc++ (Apple clang 21) and libstdc++ (GCC 13.3, clang 18).
  - (a): 1023 copies, 0 moves. (b): 0 copies, 1023 moves. 1023 = 1+2+...+512, the elements carried over at each growth.
  - (c): 1 allocation, capacity 1000, 0 copies and 0 moves.
  - Why (a) copies: when one element is added at the end, the standard requires "no effects" if an exception is thrown, as long as `T` is copyable or has a non-throwing move ([vector.modifiers] p1). A move that may throw could leave the old block half-emptied. For these copyable test types on the named libraries, removing `noexcept` changes relocation from moves to copies.
  - Why (c) works: after `reserve(n)`, capacity is at least n and no reallocation happens until the size would exceed the capacity ([vector.capacity] p4, p7).
  - The growth factor (doubling here) is not in the standard; it is a library choice. Count it, do not assume it.
  - Holds for / re-check: libc++ (Apple clang 21) and libstdc++ (GCC 13.3 and clang 18.1.3, Ubuntu 24.04 container), checked 2026-11-18, 3 runs each, identical. Another standard library may grow by a different factor (so a different allocation count) or choose differently between copy and move. The exception guarantee is standardized; these exact copy/move counts are observations. Noncopyable types with potentially throwing moves can still be moved, with unspecified effects if such a move throws. Re-check: run `vector_grow.cpp` against the library you ship.
</details>

### Card M1-PB10 · Apply
Q: A `Tile` holds `int left = 12, right = 34`; its destructor increments a counter. Create it with `make_shared<Tile>()`, then build `shared_ptr<int> left(owner, &owner->left)` and `right(owner, &owner->right)`. This is the *aliasing constructor*: it takes an existing owner and a pointer to expose. Predict whether the two stored pointers are equal and whether they share ownership. Then predict the owner count and the destructor count after resetting `owner`, then `left`, then `right`. Write the small program yourself, then check with `alias_lifetime.cpp`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/alias_lifetime.cpp -o "${TMPDIR:-/tmp}/alias_lifetime" && "${TMPDIR:-/tmp}/alias_lifetime"
   ```
   Answer format: pointers equal, same owner, owner count; then destructions and owners after each reset.
<details><summary>Answer</summary>
Key:

  - Initially: different stored pointers, same ownership group, three owners. Test ownership with `!left.owner_before(right) && !right.owner_before(left)`; pointer equality answers a different question.
  - Reset `owner`: zero destructions, two owners, the exposed values are still `12,34`.
  - Reset `left`: zero destructions, one owner, the remaining value is `34`.
  - Reset `right`: one destruction of the whole `Tile`. The aliases keep the containing object alive; they do not delete its integer members one by one.
  - Use an alias tied to the containing object's ownership to expose a stable member that must outlive the original handle. The exposed address must stay valid for that ownership lifetime.
  - Holds for / re-check: single-threaded C++20, `-O2 -Wall -Wextra`, Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64); [util.smartptr.shared.const] p14-17, [util.smartptr.shared.dest] p1, [util.smartptr.shared.obs] p12, p18. The member addresses stay stable here. Re-check validity when fixed members become storage that can move; the alias does not certify an arbitrary supplied address.
</details>

### Card M1-PB11 · Spot the trap
Q: A reviewer uses `if (p)` as proof that a `shared_ptr` keeps its pointed-to object alive. Predict bool and `use_count()` for two cases. (A) An alias made from an empty `shared_ptr<int>` and the address of a local `int` that outlives the alias. (B) An alias made from a non-empty `make_shared<int>` owner and `nullptr`, after the original owner has been reset. State exactly what bool checks, then verify with `alias_truth.cpp`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/alias_truth.cpp -o "${TMPDIR:-/tmp}/alias_truth" && "${TMPDIR:-/tmp}/alias_truth"
   ```
   Answer format: bool and owner count for (A) and (B), then one line on what bool checks.
<details><summary>Answer</summary>
Key:

  - (A): bool `1`, owner count `0`. Its pointer is non-null, but the alias owns nothing; the local integer is not kept alive by it.
  - (B): bool `0`, owner count `1`. It keeps the heap integer alive while exposing a null pointer. Do not dereference it.
  - Bool is exactly `get() != nullptr`. It does not test ownership or the pointee's lifetime. A non-null borrowed alias needs a separate lifetime guarantee.
  - Checking the count as well is still not a general validity test for the exposed pointer: the aliasing constructor takes the address separately from the owner. Know what the owner keeps alive and how the address relates to it.
  - Holds for / re-check: single-threaded C++20, the same two toolchains and flags as card M1-PB10; [util.smartptr.shared.const] p14-17, [util.smartptr.shared.obs] p12, p17. The local stays alive throughout; no dangling pointer is read. Re-check the lifetime contract when an alias crosses a callback or storage boundary. Do not turn these single-thread counts into a synchronization test.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
