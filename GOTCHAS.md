# C++ gotchas — compact reference

Use this after attempting the cards. Each row points to the lesson that teaches
and tests the rule. The examples and their build commands live there; this
page contains no separate code snippets to maintain.

| Watch for | Rule to retrieve | Lesson |
|---|---|---|
| A moved-from object | Unless a stronger postcondition is specified, moved-from standard-library objects remain valid with an unspecified value; for your own classes, check their move contract. A moved-from `unique_ptr` is empty. | [Moves](modules/01-foundations/lessons/lesson-01-move-semantics.md), [pointers](modules/01-foundations/lessons/lesson-02-smart-pointers.md) |
| `return std::move(local)` | When returning a local of the function's return type by value, prefer `return local;` so NRVO remains possible. | [Moves](modules/01-foundations/lessons/lesson-01-move-semantics.md) |
| Move constructor without `noexcept` | A vector may copy during growth when a move can throw and copying is available, so promise `noexcept` only when the move can honor it. | [Moves](modules/01-foundations/lessons/lesson-01-move-semantics.md) |
| Stored lambda with reference capture | Check that the referent is alive when the lambda runs, since leaving the function does not by itself prove a reference parameter dangles and a dangling reference has no predictable output. | [Lambdas](modules/01-foundations/lessons/lesson-03-auto-lambdas.md) |
| `auto` and proxy references | Plain `auto` usually makes a new value, but `vector<bool>` returns a proxy whose copied form can still change the element. | [Auto](modules/01-foundations/lessons/lesson-03-auto-lambdas.md) |
| Borrowed views | Keep a view’s storage alive before reading it, and remember that a dangling `string_view` can still report its size while a substring view has no guaranteed terminator at its chosen end. | [Views](modules/04-stl-modern/lessons/lesson-04-string_view-span-ctad.md) |
| Plain data beside an atomic flag | A release store and acquire load can publish plain data when the load reads that store or its release sequence, but acquire alone on the same atomic does not do so. | [Atomics](modules/03-concurrency/lessons/lesson-02-atomics-memory-model.md) |
| Sanitizer results | A sanitizer report exposes a problem on a path it ran, while a clean run cannot prove the absence of races or lifetime errors. | [Sanitizers](modules/07-architecture/lessons/lesson-02-sanitizers-analysis.md) |
| `std::endl` in a loop | `std::endl` writes a newline and asks for a flush, so inspect the stream buffer and measure instead of assuming every flush makes one system call. | [Hot path](modules/06-hft-patterns/lessons/lesson-02-hot-path-design.md) |
| Reusing storage | C++20 lifetime rules differ from older rules, so keep a pointer returned by placement construction when available and use `std::launder` only when its preconditions apply. | [Lifetime](modules/05-memory-perf/lessons/lesson-01-allocators-pmr.md) |
| Fixed pool under a container | Classify pool requests by size and alignment because a list node may differ from its element, and an assertion alone gives no production fallback. | [Pool lab](modules/05-memory-perf/lessons/lesson-05-pool-allocator-lab.md) |
| `[[likely]]` and prefetch | Treat `[[likely]]` and prefetch as hints, then inspect the generated program and measure the workload before claiming a gain. | [Hints](modules/06-hft-patterns/lessons/lesson-03-measurement-bypass.md), [prefetch](modules/06-hft-patterns/lessons/lesson-04-prefetching.md) |
| CMake settings | Prefer target usage requirements and test a consumer because directory commands have scope and timing while global C++ flags use another mechanism. | [CMake](modules/07-architecture/lessons/lesson-04-modern-cmake.md) |
| C++ library ABI | Match the ABI of objects that share C++ types because `_GLIBCXX_USE_CXX11_ABI` is a libstdc++ setting rather than a portable switch. | [Builds](modules/07-architecture/lessons/lesson-04-modern-cmake.md) |

For the normative wording behind ownership, lifetime and synchronization, see
[the C++ working draft](https://eel.is/c++draft/) and the source links in each module’s `cards.json`.
