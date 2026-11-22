# Lesson 06 — Practice Bank (optional): Reading the Draft, Closed Sets of Types

**Time: about 50 minutes** for 9 cards (4 + 6 + 7 + 5 + 5 + 5 + 7 + 6 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

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

### Card M4-PB3 · Apply
Q: An unsigned configuration field must contain exactly one decimal integer with no surrounding text. `parse_boundary.cpp` calls `from_chars` into a `uint32_t` (initially 99) for "37", "37ms", " 37", "+37", "4294967296" and "". Predict the error category, characters consumed and output value for each. Which inputs leave the output untouched, and where does `ptr` stop on overflow? Then run `parse_rules.cpp`, which tries "+37" with a signed `int` and " 37" with `strtol`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary"
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_rules.cpp -o "${TMPDIR:-/tmp}/parse_rules" && "${TMPDIR:-/tmp}/parse_rules"
   ```
   Answer format: one line per input (category, consumed, value), then where `ptr` stops on overflow.
<details><summary>Answer</summary>
Key:

  - Integer `from_chars` accepts no '+' sign for any integer type and skips no leading whitespace (unlike `strtol`). Here "+37" and " 37" are invalid, consume 0 characters and leave the value at 99.
  - "4294967296": `result_out_of_range`, consumed 10, value stays 99. `ptr` advances past all ten digits even though the value cannot be represented.
  - "37": ok, consumed 2, value 37. Empty input: invalid, consumed 0, value stays 99.
  - "37ms": ok, consumed 2, value 37; apply the `ec`/`ptr` whole-field check from lesson 01 (CONCEPT C3, `parseQty`).
  - Holds for / re-check: Apple clang 21 with libc++, C++20, the decimal `uint32_t` overload ([charconv.from.chars]). This policy deliberately rejects signs, whitespace and trailing text; change the policy explicitly before comparing a different parser. Re-check the boundary and overflow cases after library changes.
</details>

### Card M4-PB4 · Spot the trap
Q: A parse helper promises "return false and leave the old setting untouched on failure." It parses directly into the caller's `uint32_t`, then checks both the error and full consumption. With old value 77 and input "37ms", predict the returned bool and the final value. Write a fix using a temporary, then compare the `direct` and `transactional` helpers in the same probe (last line of its output):
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary"
   ```
   Answer format: bool and value for each helper, then what a test must assert.
<details><summary>Answer</summary>
Key:

  - `direct` returns false but leaves the value at 37: the numeric-prefix parse succeeded before the wrapper rejected the suffix.
  - `transactional` returns false and leaves the value at 77. It parses into a temporary and assigns to the caller only after all checks pass.
  - Testing only the returned bool misses the broken failure contract. Assert both the rejection and the unchanged state.
  - Holds for / re-check: Apple clang 21 with libc++, C++20; the inputs come from string literals. Any extra policy checks must come before committing the temporary; rerun the rejection tests when the wrapper changes.
</details>

### Card M4-PB5 · Apply
Q: A diagnostic panel writes a hexadecimal device code into the first four bytes of `char out[6] = {'?','?','?','?','!','\0'}` with `to_chars(out, out+4, 0xbeefu, 16)`. Predict success, the returned length and the fifth byte. Then predict what a length-bounded view displays, and what a C-string consumer (one that reads until a zero byte) displays. Build your own check, then compare with `hex_buffer.cpp`. All six bytes are initialized, so the C-string read itself is safe:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/hex_buffer.cpp -o "${TMPDIR:-/tmp}/hex_buffer" && "${TMPDIR:-/tmp}/hex_buffer"
   ```
   Answer format: success, length, fifth byte; then what each consumer displays and why.
<details><summary>Answer</summary>
Key:

  - Success; length `4`; `ptr == out+4`; the fifth byte stays `!`.
  - The bounded view displays `beef`; the C-string consumer displays `beef!`. `to_chars` writes the digits and adds no null terminator.
  - Carry the returned length into the consumer. If a C-string interface is required, reserve a separate byte and terminate explicitly after checking success. An exact-fit digit range does not give you that spare byte.
  - Holds for / re-check: C++20 integer `to_chars`, base 16, `-O2 -Wall -Wextra`, Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64); [charconv.to.chars] p1, p4-6. The buffer has a deliberate terminator at index 5; that does not license C-string reads from an arbitrary output buffer. Re-check capacity and termination when the consumer changes.
</details>

### Card M4-PB6 · Spot the trap
Q: A helper treats `to_chars(...).ptr == last` as proof that conversion failed. Write `0xbeefu` in base 16 into ranges of capacity `0, 3, 4, 5`, each inside a larger initialized array. Predict the error category, the returned offset and `ptr == last` for each. Which field actually decides success? For a too-small range, may a portable test require the bytes inside it to stay unchanged? Run `hex_capacity.cpp`, which checks guard bytes outside the offered range and deliberately does not inspect the bytes inside a failed range:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/hex_capacity.cpp -o "${TMPDIR:-/tmp}/hex_capacity" && "${TMPDIR:-/tmp}/hex_capacity"
   ```
   Answer format: one line per capacity (category, offset, at end), then the deciding field, then yes or no for the interior bytes.
<details><summary>Answer</summary>
Key:

  - Capacity `0`: `value_too_large`, offset `0`, at end `1`. Capacity `3`: `value_too_large`, offset `3`, at end `1`. (The program prints `too_small` for `errc::value_too_large`.)
  - Capacity `4`: success, offset `4`, at end `1`. Capacity `5`: success, offset `4`, at end `0`. Both successful results spell `beef`.
  - The exact-fit success disproves the helper. Use `ec == std::errc{}` to decide success, then use `ptr` for the length.
  - No. On failure the standard leaves the contents of the offered range unspecified. Do not grade those bytes or treat them as a usable partial result. The guard bytes next to the range stay unchanged in all four cases; outside guards check a different property.
  - Holds for / re-check: the same two toolchains and flags as card M4-PB5; [charconv.to.chars] p1. All ranges, including the empty one, are valid and point into a live array. The error and offset contract is standard; the bytes left inside a failed range may differ by library version. Re-check the wrapper's error test when its conversion API changes.
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

### Card M4-PB8 · Spot the trap
Q: A checkpoint writer serializes the two doubles `1.0` and `std::nextafter(1.0, 2.0)` (the next representable double toward 2). It uses `to_chars` with general format and precision 6. A replacement uses the overload with no format or precision argument. Predict the number of distinct strings and exact numeric round trips for each writer. Build a test that checks both conversion errors and full consumption when reading back; compare with:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/decimal_roundtrip.cpp -o "${TMPDIR:-/tmp}/decimal_roundtrip" && "${TMPDIR:-/tmp}/decimal_roundtrip"
   ```
   Answer format: distinct strings and exact round trips for each writer, then what to validate first.
<details><summary>Answer</summary>
Key:

  - Precision 6: 1 distinct string and 1 exact round trip out of 2. Shortest form: 2 distinct strings and 2 exact round trips.
  - The precision-limited text merges these neighbouring values. A readable display is not necessarily a lossless checkpoint.
  - The no-format `to_chars` overload picks a shortest representation that recovers the value through the matching `from_chars` on the same implementation. Validate the round trip before comparing formatter costs.
  - This test uses two finite positive values. It does not show that NaN payloads or signed-zero bits survive, or that every parser reads the text back.
  - Holds for / re-check: IEEE-754 double with 53 significand bits (checked at compile time), Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`; [charconv.to.chars] p2, p7, p13 (C++23 draft N4950). Each writer and reader uses the same library. Re-check when changing precision, format, parser or the checkpoint's equality requirements.
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

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
