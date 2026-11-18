# Lesson 06 — Practice Bank (optional): Reading the Draft, Closed Sets of Types

**Time: about 10 minutes** for 2 cards (4 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: add one type to a variant and the compiler finds every place you forgot; add a catch-all and a triangle's area prints as 0.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++).

## Cards

### Card M4-PB1 · Recall
Q: Before relying on a C++26 hive in a design, sort these into "the draft guarantees it" and "the draft leaves it to the implementation": (1) an erase keeps pointers to the other elements valid; (2) erased slots get filled again by later inserts; (3) erasing does not move the later elements; (4) how the container finds erased slots while iterating; (5) how big each new block is. Look it up in [hive.overview] and [hive.modifiers] of the current draft.
   Answer format: "guaranteed:" with numbers, then "not guaranteed:" with numbers.
<details><summary>Answer</summary>
Key:

  - Guaranteed (1): erase "Invalidates references, pointers and iterators referring to the erased elements" only (and `end()` if the last element is erased) ([hive.modifiers] p17).
  - Guaranteed (3): erased locations are skipped during iteration, "as opposed to relocating subsequent elements" ([hive.overview] p3).
  - Not guaranteed (2): insertion position "is determined by the container", and insertion "may re-use" erased locations ([hive.overview] p1). "May" is permission, not a promise. Measure your library if density matters.
  - Full answer in the asked format: `guaranteed: 1, 3` and `not guaranteed: 2, 4, 5`.
  - Left to the implementation (4) and (5): the skip technique is "unspecified" (p3); the block growth factor is "implementation-defined" (p4).
  - Technique: for any new library type, read "may" versus "shall" before building on a property.
  - Holds for / re-check: draft wording (N5054, 2026-07). Re-check against the final C++26 text and your library's documentation.
</details>

### Card M4-PB2 · Spot the trap
Q: A shape type is `std::variant<Circle, Rect>` and `area()` handles it with `std::visit` and an overload set of two lambdas. Add a third alternative `Triangle`. What happens at build time? Then add a catch-all lambda `[](const auto&) { return 0.0; }` and build again. Check with the three builds, then build and run the catch-all version:
   ```sh
   clang++ -std=c++20 -fsyntax-only modules/04-stl-modern/lab/practice/variant_new_type.cpp
   clang++ -std=c++20 -fsyntax-only -DADD_TYPE modules/04-stl-modern/lab/practice/variant_new_type.cpp
   clang++ -std=c++20 -fsyntax-only -DADD_TYPE -DCATCH_ALL modules/04-stl-modern/lab/practice/variant_new_type.cpp
   clang++ -std=c++20 -DADD_TYPE -DCATCH_ALL -DRUN modules/04-stl-modern/lab/practice/variant_new_type.cpp -o "${TMPDIR:-/tmp}/variant_new_type" && "${TMPDIR:-/tmp}/variant_new_type"
   ```
   Answer format: one line per build, then one line on what the catch-all costs you.
<details><summary>Answer</summary>
Key:

  - Without the new type it compiles. With it, the build fails: libc++ says "`std::visit` requires the visitor to be exhaustive". The standard mandates that the visitor call is valid for every alternative.
  - That error is a feature: the compiler lists every place you forgot to handle the new type.
  - With the catch-all it compiles again, and `area()` silently returns 0.0 for a `Triangle`: the run prints `area(Rect{3,4})=12` and `area(Triangle{3,4})=0`. A catch-all turns a compile error into a wrong answer. Use one on purpose only.
  - Holds for / re-check: every conforming library rejects the non-exhaustive visit ([variant.visit], Mandates); the message text is libc++-specific. Re-check by compiling on your library.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
