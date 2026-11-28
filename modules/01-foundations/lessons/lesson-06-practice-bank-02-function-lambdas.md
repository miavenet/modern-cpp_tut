# Lesson 06 — Practice Bank, part 2 of 4 (optional): `std::function` and Lambdas

**Time: about 28 minutes (estimate)** for 4 cards (10 + 6 + 4 + 8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: where the small buffer ends, what the standard promises about it, and what a closure really is.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

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
  - Holds for / re-check: measured on libc++ (Apple clang 21) and libstdc++ 13. The `noexcept` on the move constructor is required by the C++20 and C++23 drafts, so (3) holds for any conforming library; (1) and (2) depend on the target being too big for the small buffer. Re-check by running both programs.
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

---

## Close

Next: part 3, `lesson-06-practice-bank-03-growth-sharing.md` (about 22 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
