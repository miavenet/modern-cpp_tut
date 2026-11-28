# Lesson 06 — Practice Bank, part 3 of 4 (optional): Vector Growth and Shared Ownership

**Time: about 28 minutes (estimate)** for 4 cards (12 + 6 + 4 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: counting allocations as a vector grows, what an aliasing `shared_ptr` keeps alive, and which expression moves a lock's ownership.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

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

### Card M1-PB12 · Spot the trap
Q: A single-thread trace uses a stack-local `Gate` whose `lock()` and `unlock()` only increment counters. `unique_lock<Gate> first(g)` acquires it. Predict ownership and the lock/unlock counts after (1) `(void)std::move(first)`, (2) constructing `second(std::move(first))` in an inner scope, (3) leaving that inner scope, and (4) destroying `first`. Write the trace yourself, then check with `lock_transfer.cpp`. What responsibility moved, and which expression actually moved it?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/lock_transfer.cpp -o "${TMPDIR:-/tmp}/lock_transfer" && "${TMPDIR:-/tmp}/lock_transfer"
   ```
   Answer format: owns and lock/unlock counts after each of the four steps, then the responsibility that moved and the expression that moved it.
<details><summary>Answer</summary>
Key:

  - Cast only: `first` owns=1; locks=1, unlocks=0. `std::move` supplies an rvalue reference; discarding that expression invokes no move constructor.
  - Construct `second`: `first` owns=0, `second` owns=1; locks=1, unlocks=0. Construction transfers responsibility for unlocking; it does not acquire the gate a second time.
  - Inner exit: locks=1, unlocks=1. Outer exit: still locks=1, unlocks=1. The moved-from `first` no longer unlocks on destruction.
  - The resource here is a lock-ownership obligation on a stack-local gate. Identify the expression that changes ownership.
  - Holds for / re-check: Apple clang 21.0.0 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Linux aarch64), C++20, `-O2 -Wall -Wextra`; [thread.lock.unique.cons] p2-3, p17, p22 and [forward] p9 for the `std::move` cast (C++23 draft N4950). A single-thread counter test double, not a concurrent mutex implementation; the gate outlives both owners; move construction is tested, not move assignment into an already owning destination. Re-run `lock_transfer.cpp`; for a different owner type, inspect its move and destruction contracts before predicting the counters.
</details>

---

## Close

Next: part 4, `lesson-06-practice-bank-04-rehash-handles.md` (about 9 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
