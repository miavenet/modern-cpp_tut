# Lesson 06 — Practice Bank, part 2 of 2 (optional): Numbers to Text and Back at the Edges

**Time: about 28 minutes (estimate)** for 5 cards (7 + 5 + 5 + 5 + 6 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: `from_chars` boundaries and failure, `to_chars` capacity, and exact round trips.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++).

## Cards

### Card M4-PB3 · Apply
Q: An unsigned configuration field must contain exactly one decimal integer with no surrounding text. `parse_boundary.cpp` calls `from_chars` into a `uint32_t` (initially 99) for "37", "37ms", " 37", "+37", "4294967296" and "". Predict the error category, characters consumed and output value for each. Which inputs leave the output untouched, and where does `ptr` stop on overflow? Then run `parse_rules.cpp`, which tries "+37" with a signed `int` and " 37" with `strtol`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary" boundaries
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
Q: A parse helper promises "return false and leave the old setting untouched on failure." It parses directly into the caller's `uint32_t`, then checks both the error and full consumption. With old value 250 and input "64kb", predict the returned bool and the final value. Write a fix using a temporary, then compare the `direct` and `transactional` helpers in the probe's `transactional` mode:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary" transactional
   ```
   Answer format: bool and value for each helper, then what a test must assert.
<details><summary>Answer</summary>
Key:

  - `direct` returns false but leaves the value at 64: the numeric-prefix parse succeeded before the wrapper rejected the suffix.
  - `transactional` returns false and leaves the value at 250. It parses into a temporary and assigns to the caller only after all checks pass.
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

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
