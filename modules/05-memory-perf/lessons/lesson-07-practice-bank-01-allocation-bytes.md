# Lesson 07 — Practice Bank, part 1 of 9 (optional): What an Allocation Really Costs

**Time: about 26 minutes (estimate)** for 3 cards (6 + 10 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: memory reuse in a monotonic resource, and the real heap bytes per container node.

Hook: How many heap bytes does one small container element really cost?

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

### Card M5-PB2 · Spot the trap
Q: A service uses one `std::pmr::monotonic_buffer_resource` per session. Its hot loop allocates a 64-byte scratch object and deallocates it each iteration, "so memory use stays flat". A session lasts hours. What happens, and what does the standard say?
   Answer format: what happens, the rule, and one fix.
<details><summary>Answer</summary>
Key:

  - `do_deallocate` on a monotonic resource has "Effects: None", and "Memory used by this resource increases monotonically until its destruction." So the loop grows memory without bound.
  - Measured here with a counting upstream: after 100k, 200k and 300k alloc/free pairs, upstream bytes were about 8.4 MB, 16.8 MB and 33.6 MB (buffers grow geometrically).
  - Fix: call `release()` at a safe point (for example per message batch), use a fresh resource per unit of work, or use a pool resource (`unsynchronized_pool_resource`) when blocks are really freed and reused.
  - Holds for / re-check: the no-op deallocate is required by the standard, so it holds on every implementation; the growth sizes are libc++'s choice and may differ elsewhere: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ 13 gave about 6.8, 15.3 and 23.0 MB at the same points. Re-check the growth with `clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/pmr_monotonic.cpp -o "${TMPDIR:-/tmp}/pmr_monotonic" && "${TMPDIR:-/tmp}/pmr_monotonic"`.
</details>

### Card M5-PB4 · Apply
Q: A record is `struct Rec { uint32_t id; uint64_t qty; };`, so the raw data is 12 bytes. Predict the heap bytes per element for `std::list<Rec>`, `std::set<Rec>`, `std::deque<Rec>`, and `std::vector<Rec>` with and without `reserve(n)`. Then run the program, which counts bytes requested by each container and bytes malloc actually granted (`malloc_size`):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/bytes_per_elem.cpp -o "${TMPDIR:-/tmp}/bytes_per_elem" && "${TMPDIR:-/tmp}/bytes_per_elem"
   ```
   Answer format: `sizeof(Rec)`, then bytes per element for each container.
<details><summary>Answer</summary>
Key:

  - Padding first: `sizeof(Rec)` is 16, not 12 (`alignof` 8): the 8-byte field must be 8-aligned. With only these two fields, swapping their order cannot help: the size must be a multiple of the 8-byte alignment, and 12 rounds up to 16.
  - Requested, here (libc++), at n = 1000, 50000 and 1000000: list 32 (node = prev and next pointers + Rec), set 48 (node = left, right and parent pointers and a colour flag, padded, + Rec), deque 16.04-16.42, vector after reserve 16.00.
  - Vector without reserve keeps its growth slack: 16.38, 20.97, 16.78 bytes/element at the three sizes, depending on how far n is from the last capacity step.
  - Granted vs requested, macOS malloc: rounding showed up only for the vector after reserve (16.38, 16.06, 16.01 granted vs 16.00 requested); the node containers matched exactly. `malloc_size` does not include malloc's own bookkeeping, so it is still not the full process cost.
  - These are counts, not timings: two runs gave identical numbers.
  - Holds for / re-check: libc++ node layouts and growth policy, macOS malloc size classes, Apple clang 21, `-O2`. Other standard libraries and allocators differ: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), the list, set and vector requests were the same, deque requested 16.41-17.01, and glibc rounded the nodes too (list granted 40, set 56). `bytes_per_elem.cpp` builds on macOS only (`malloc_size` is a macOS call); the Linux numbers come from `bytes_per_elem_glibc.cpp` in the same folder, the same program with glibc's `malloc_usable_size`: `g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/bytes_per_elem_glibc.cpp -o "${TMPDIR:-/tmp}/bytes_per_elem_glibc" && "${TMPDIR:-/tmp}/bytes_per_elem_glibc"`. Re-check by running the matching program on the target.
</details>

### Card M5-PB5 · Apply
Q: A colleague estimates the memory of a node-based container as `N * sizeof(value_type)`. How do you measure the real bytes per element on your machine, in two steps? Then run this program, which does both steps for three containers, and read what each step shows:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes.cpp -o "${TMPDIR:-/tmp}/node_bytes" && "${TMPDIR:-/tmp}/node_bytes"
   ```
   Answer format: step 1 and step 2, one line each, with what each showed in your run.
<details><summary>Answer</summary>
Key:

  - Step 1: count what the container asks the allocator for, with a counting allocator. You do not need to guess the node layout. Here a `std::list` node for a 40-byte value asks for 56 bytes; a `std::map<int, 40-byte value>` node asks for 80; a `std::list<std::uint64_t>` node asks for 24.
  - Step 2: ask the allocator how big each live node's block really is. Here `malloc_size` on the node's own pointer gives 64 for the 56-byte request, 80 for 80, and 32 for 24.
  - So the 8-byte value really takes a 32-byte block per element here, 4 times the estimate (not counting the allocator's own bookkeeping outside the block, which this test does not see).
  - Holds for / re-check: node layout is a library detail (here libc++ 220106); rounding is allocator-specific, and `malloc_size` is a macOS call. Other allocators have their own size query, but it may not measure the same thing (for example, it may leave out a per-block header). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), glibc's `malloc_usable_size` gave 24 for the 24-byte node, but consecutive blocks sat 32 bytes apart: it leaves out the 8-byte header. Read its documentation, or measure memory growth over many nodes instead. `node_bytes.cpp` builds on macOS only; the Linux numbers come from `node_bytes_glibc.cpp` in the same folder (the same steps with `malloc_usable_size`, plus the address step between consecutive blocks): `g++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/node_bytes_glibc.cpp -o "${TMPDIR:-/tmp}/node_bytes_glibc" && "${TMPDIR:-/tmp}/node_bytes_glibc"`. Re-check by running the matching program on each library and allocator.
</details>

---

## Close

Next: part 2, `lesson-07-practice-bank-02-layout-alignment.md` (about 24 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
