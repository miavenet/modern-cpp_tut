# Lesson 05 — Practice Bank, part 4 of 4 (optional): What an Object's Bytes Hold

**Time: about 11 minutes (estimate)** for 2 cards (4 + 7 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: uninitialized reads in the C++26 draft, and a `memcpy` of a record that points into itself.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; diagnostics and listings can change with any compiler release.

## Cards

### Card M2-PB7 · Recall
Q: In the C++26 working draft, a local `int a;` and a local `int b [[indeterminate]];` are both read before any write, and so is an `int` obtained with `new int`. For each read, is the behaviour undefined, erroneous, or fine? What is an implementation allowed to do on erroneous behaviour? Look it up in [basic.indet] and [defns.erroneous].
   Answer format: one word per read, then what the implementation may do.
<details><summary>Answer</summary>
Key:

  - `a`: erroneous behaviour. Its bytes start with "erroneous values ... determined by the implementation independently of the state of the program".
  - `b` and the `new int`: undefined behaviour. Dynamic storage and variables marked `[[indeterminate]]` start with indeterminate values.
  - Erroneous behaviour is "well-defined behavior that the implementation is recommended to diagnose"; it "is permitted to issue a diagnostic and is permitted to terminate the execution".
  - So `[[indeterminate]]` is an opt-out that gives back UB for that one variable.
  - Holds for / re-check: C++26 working draft N5054 (2026-07). Earlier standards (C++23 and before) make all three undefined. Re-check against the published standard when it appears ([basic.indet]).
</details>

### Card M2-PB12 · Spot the trap
Q: A record contains bytes {A,B,C,D} and a pointer selecting its own B. A second record receives a `memcpy` of the first while both stay alive. The source's B is then changed to Z. Predict: is the pointer-bearing type trivially copyable and standard-layout? What are the destination's local byte and selected byte? Repeat with the selection stored as the integer offset 1, bounds-checked before indexing:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/relocate_record.cpp -o "${TMPDIR:-/tmp}/relocate_record" && "${TMPDIR:-/tmp}/relocate_record"
   ```
   Answer format: the two trait values, local and selected byte for each version, then what the traits do not certify.
<details><summary>Answer</summary>
Key:

  - Both traits print 1 for the pointer-bearing record. Yet the copied pointer still selects the source byte: destination local=B, selected=Z, points_to_source=1.
  - The offset version selects B from its own copied bytes after the source becomes Z.
  - Trivial copying preserves this pointer value; it does not rebase an internal address. These traits alone do not certify a relocatable record format.
  - For this bounded byte-array design, use a validated offset resolved against the destination. That does not by itself establish a portable file format or an inter-process synchronization protocol.
  - Holds for / re-check: Apple clang 21, C++20. Both records stay alive, so no dangling pointer is dereferenced; this is a single-process relocation test, not a shared-memory concurrency test. Re-check offset units, bounds and format compatibility when the record changes.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain change, re-run a card's check before trusting its result.
