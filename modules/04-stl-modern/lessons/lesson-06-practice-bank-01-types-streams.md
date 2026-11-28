# Lesson 06 — Practice Bank, part 1 of 2 (optional): Containers, Closed Sets of Types and Stream State

**Time: about 22 minutes (estimate)** for 4 cards (4 + 6 + 7 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: what the hive draft guarantees, variant visitors, heterogeneous lookup, and a sticky `std::hex`.

Hook: A new shape tests whether every visitor still handles the whole type set.

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

### Card M4-PB7 · Apply
Q: A set stores `Key{int value}` objects. `Key(int)` is an implicit conversion and increments a construction counter. The hash and equality objects accept both `Key` and `int`, with consistent results for equal values. The set contains 4 and 12; after resetting the counter, it probes with integers 4, 8 and 12. "Heterogeneous lookup" means searching with a different type from the stored key. Predict matches and key constructions when both function objects declare `using is_transparent = void`, and when neither does. Implement the probe, then compare with both builds:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/lookup_materialization.cpp -o "${TMPDIR:-/tmp}/lookup_materialization" && "${TMPDIR:-/tmp}/lookup_materialization"
   clang++ -std=c++20 -O2 -Wall -Wextra -DOPAQUE modules/04-stl-modern/lab/practice/lookup_materialization.cpp -o "${TMPDIR:-/tmp}/lookup_opaque" && "${TMPDIR:-/tmp}/lookup_opaque"
   ```
   Answer format: matches and key constructions for each build, then what enables the heterogeneous `find`.
<details><summary>Answer</summary>
Key:

  - Both builds find 2 keys. Transparent lookup constructs 0 temporary keys; the opaque build constructs 3, including one for the unsuccessful probe.
  - Merely providing overloads for integers does not enable the heterogeneous `find` overload. The hash and equality transparency markers enable it; their operations must also support the probe type correctly.
  - Count work at the lookup boundary separately from set construction. A matching hit count would not reveal these extra conversions, and a construction count is not a speed measurement.
  - Holds for / re-check: C++20 unordered lookup ([unord.req.general] p242, C++23 draft N4950), Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), `-O2 -Wall -Wextra`. The counter starts after insertion. An explicit constructor would make the opaque form fail instead of converting implicitly; re-check when the key, hash, equality or library changes.
</details>

### Card M4-PB9 · Spot the trap
Q: A diagnostic helper writes `out << std::hex << x` to a caller-supplied stream. An initially decimal `ostringstream` writes 15, calls the helper with 15, then writes 15 again, with spaces between values. Predict the resulting text. Does a separate fresh stream also change? Write a helper that restores the old flags and compare:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/stream_state.cpp -o "${TMPDIR:-/tmp}/stream_state" && "${TMPDIR:-/tmp}/stream_state"
   ```
   Answer format: the text of the unguarded, fresh and restoring streams, then how to test such a helper.
<details><summary>Answer</summary>
Key:

  - The unguarded stream contains `15 f f`. The helper leaves hexadecimal formatting active for the next insertion.
  - The independent stream contains `15`. This state belongs to the particular stream, not every stream in the process.
  - Saving `out.flags()` and restoring it after this helper's insertion gives `15 f 15`. This helper changes only flags.
  - Test a helper by observing its caller's next operation, not just the helper's own output.
  - Holds for / re-check: single-threaded string streams without enabled stream exceptions; Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`; [fmtflags.state] p1-3, [basefield.manip] p4 (C++23 draft N4950). The simple restoration is tested on normal return only. If a helper changes precision, fill, locale or exception behaviour, widen the restoration contract and its tests; this program does not certify those cases.
</details>

---

## Close

Next: part 2, `lesson-06-practice-bank-02-charconv.md` (about 28 minutes). Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
