# Lesson 06 — Practice Bank, part 7 of 8 (optional): What Compare-Exchange Calls Equal

**Time: about 8 minutes (estimate)** for 1 card (8 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: the equality a compare-exchange uses, beyond which instruction it compiles to (part 1).

Hook: Negative zero equals positive zero, and a NaN never equals itself. Predict what a compare-exchange says about each before you run it.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain, and vary with load; counts and listings are steadier.

## Cards

### Card M3-PB18 · Spot the trap
Q: A floating-point state inspector substitutes `a.load(relaxed) == expected` for a representation comparison. On a 32-bit IEEE float target, test (A) stored negative zero `0x80000000` against positive zero `0x00000000`; (B) the same quiet NaN representation `0x7fc00031` on both sides. A NaN is a floating-point "not a number" value. Predict numeric equality, strong compare-exchange success, and `expected` afterwards. The compare-exchange's desired value is the original expected value; there are no other threads. Then check with `cas_equality.cpp`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/03-concurrency/lab/practice/cas_equality.cpp -o "${TMPDIR:-/tmp}/cas_equality" && "${TMPDIR:-/tmp}/cas_equality"
   ```
   Then read the Introduction of WG21 paper P4221R0 (open-std.org): which equality contract does its proposed read-only compare seek to preserve?
   Answer format: numeric, cas, expected and final for (A) and (B), the rule compare-exchange uses, then the contract P4221R0 keeps.
<details><summary>Answer</summary>
Key:

  - Signed zero: `numeric=1 cas=0 expected=80000000 final=80000000`.
  - Same NaN: `numeric=0 cas=1 expected=7fc00031 final=7fc00031`.
  - Strong compare-exchange compares value representations, not `operator==`. On failure it updates `expected` with the representation read. Here the zero comparison fails; the identical NaN representation succeeds. Using the strong form avoids confusing this experiment with weak compare-exchange's permitted spurious failure.
  - P4221R0 proposes a read-only comparison retaining compare-exchange's representation semantics. A numeric `==` wrapper would not meet that proposed contract for these inputs. This program uses existing C++20 APIs; it neither implements nor tests the proposed API.
  - Holds for / re-check: IEEE binary32, without fast-math, Apple clang 21.0.0 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Linux aarch64), `-std=c++20 -O2 -Wall -Wextra`; [atomics.types.operations] p23-28 (C++23 draft N4950). Fixed quiet NaN bits, no padding and no concurrent writer: do not generalize bytewise comparisons to padded classes. An instruction count cannot establish semantic interchangeability, and no performance claim follows here. Re-run the program, inspect its floating-format `static_assert`s, and reread the exact paper revision before applying its wording to an implementation.
</details>

---

## Close

Next: part 8, `lesson-06-practice-bank-08-wait-address.md` (about 10 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
