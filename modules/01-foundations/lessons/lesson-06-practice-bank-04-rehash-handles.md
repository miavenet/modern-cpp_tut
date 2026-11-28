# Lesson 06 — Practice Bank, part 4 of 4 (optional): Handles That Survive a Rehash

**Time: about 9 minutes (estimate)** for 1 card (9 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: which saved handles to `unordered_map` elements survive a rehash, in contrast to vector growth, which relocates its elements (part 3).

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

Terms. *Rehash*: the container moves to a new bucket array, here a larger one requested with `rehash()`.

## Cards

### Card M1-PB13 · Apply
Q: A registry saves pointers to seven `unordered_map<int,int>` values, then requests a larger bucket array. Before running, predict whether those pointers can still update the registry, and whether an iterator saved before the rehash would have the same permission. In `rehash_handles.cpp`, predict `changed` (saved pointers that no longer match the element's address) and `edited` (the value under key 39) after writing 901 through saved pointer 3. Which saved handles survive the rehash: pointers, references, iterators? Quote the sentence in [unord.req.general] that decides it.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/rehash_handles.cpp -o "${TMPDIR:-/tmp}/rehash_handles" && "${TMPDIR:-/tmp}/rehash_handles"
   ```
   Answer format: the printed line, then which handles survive and the deciding sentence.
<details><summary>Answer</summary>
Key:

  - `bucket_growth=1 changed=0 edited=901`. All seven element pointers survive; the saved pointer changes the value found under key 39. The program never uses an invalidated iterator.
  - [unord.req.general] p9 (C++23 draft N4950) guarantees pointer and reference stability across a rehash while invalidating iterators. This excludes a rehash that relocates the elements themselves and leaves saved pointers dangling.
  - Holds for / re-check: counts observed with Apple clang 21.0.0 and libc++ on macOS arm64 and GCC 13.3 and libstdc++ on Linux aarch64, C++20, `-O2 -Wall -Wextra`. No erase, concurrent mutation or allocator change; exact bucket counts are deliberately not graded; the pointer and reference contract is normative. Re-run `rehash_handles.cpp` after library changes, and reread the clause before claiming a replacement container has the same contract.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
