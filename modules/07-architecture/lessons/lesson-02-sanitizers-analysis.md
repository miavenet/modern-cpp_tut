# Lesson 02 — Sanitizers: Making Undefined Behaviour Visible

Time: about 30 minutes (10 minutes reading, a 5-minute run, 7 cards).

**The hook.** Two threads each add 1 to a shared `int` 100,000 times. A plain
run here printed `counter = 200000`, the expected value, despite the data
race. Correct-looking output proves nothing here. One compiler flag turns
this silent bug into a report with file and line.

**You will be able to:** pick the sanitizer for a bug class, read its report,
plan sanitizer builds for CI, and say what a clean run does and does not show.

**You need from earlier:** module 03 (threads, atomics); lesson 01 of this
module is not needed.

## Pretest — guess first

### Card M7-P3 · Pretest
Q: Thread A writes an `int` while thread B reads it, with no lock or atomic.
   Guess: (a) will your tests crash? (b) Which tool could report this race
   during a test run, and does a clean run prove there is no race?
<details><summary>Answer</summary>
Key:
  - (a) Not necessarily. A data race is undefined behaviour: no particular
    result is guaranteed. It may crash, give wrong values, or look fine.
  - (b) ThreadSanitizer (`-fsanitize=thread`) reports races that happen in an
    instrumented run. A clean run is evidence about the paths and timings that
    run exercised, not a proof that the program has no race.
</details>

---

### CONCEPT C3  Three sanitizers, three bug classes

**300 bugs in ten months.** In May 2011 four Google engineers released
AddressSanitizer and started testing the Chromium browser with it. In the
first 10 months it found over 300 previously unknown bugs: 210
heap-use-after-free, 73 heap-buffer-overflow, some that "could have
potentially led to security vulnerabilities".

Older checkers such as Valgrind and Dr. Memory slowed programs 20x and 10x on
their benchmark suite. ASan averaged 73%: the compiler adds a quick check of
"shadow memory" before each access, and the allocator puts poisoned zones
around every block and delays reusing freed memory.

Speed was the point. Most reports came from ordinary unit tests and fuzzing,
and a few from people simply using an instrumented browser, "which would not
have been possible with a significantly slower tool".

But notice what the shadow memory records: whether each byte is safe to
access. Two threads racing on a valid `int` break no such rule, which is why
races need a different checker.

**Words.** *Undefined behaviour* (UB) is code the standard gives no meaning
to: a data race, reading freed memory, signed overflow. The compiler may
assume it never happens. A *sanitizer* is a compiler option that adds checks
to your program; when a check fails at run time it prints a report.

Without instrumentation, you often investigate after corruption becomes
visible. A sanitizer instruments the build to catch supported errors closer
to the operation that caused them; it also works with C++98 code.

| flag | name | reports (from the Clang docs) |
|---|---|---|
| `-fsanitize=address` | ASan | out-of-bounds, use-after-free, double free, ... |
| `-fsanitize=thread` | TSan | data races |
| `-fsanitize=undefined` | UBSan | undefined behaviour such as signed overflow |
| `-fsanitize=memory` | MSan | reads of uninitialised memory (Linux, NetBSD, FreeBSD only) |

**See it yourself.** Files are in `modules/07-architecture/lab/sanitizers/`;
each file's first lines hold its command. Use after free (`uaf.cpp` reads
`o->price` after `delete o`):

```sh
clang++ -std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
  modules/07-architecture/lab/sanitizers/uaf.cpp -o "${TMPDIR:-/tmp}/uaf" && "${TMPDIR:-/tmp}/uaf"
```

The report (trimmed) names the bug, the read, and both stacks:

```
ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000958 ...
READ of size 8 at 0x602000000958 thread T0
freed by thread T0 here:
previously allocated by thread T0 here:
SUMMARY: AddressSanitizer: heap-use-after-free uaf.cpp:10 in main
```

Without the flag, the same program printed `price = 0.000000` and exited 0
on the test machine. `race.cpp` under TSan printed `WARNING:
ThreadSanitizer: data race` and `SUMMARY: ... race.cpp:11`. `overflow.cpp`
under UBSan printed `runtime error: signed integer overflow: 2147483647 + 1
cannot be represented in type 'int'`.

**Limits on this Mac (Apple clang 21, arm64).** `-fsanitize=memory` is
rejected: "unsupported option '-fsanitize=memory' for target
'arm64-apple-darwin...'". ASan's leak detection printed "detect_leaks is not
supported on this platform". A program reading an uninitialised `int` ran
clean under ASan: ASan watches whether memory may be accessed, not whether it
was initialised.

**The trap.** "The sanitizer run was clean, so the code is correct." Each
sanitizer checks only what that run executed.

**Rule:** match the sanitizer to the bug class, and treat a clean run as
evidence about the tested paths only.

| tool | catches in an exercised run | does not establish | first choice when |
|---|---|---|---|
| ASan | invalid memory access such as use-after-free | freedom from data races | pointers or bounds are suspect |
| TSan | observed data races | valid ownership of freed memory | threads share mutable state |
| UBSan | selected undefined operations such as signed overflow | absence of unexecuted bugs | arithmetic or casts are suspect |
| static analysis | warnings from paths it can reason about without running | complete runtime coverage | review before tests run |

Start with the tool that observes the suspected bug class; a clean ASan run
offered as proof of race freedom is the wrong-tool smell.

### Card M7-J2 · Apply
Q: CI runs this program under TSan with no arguments and passes. Production
   always starts it with `--dropcopy`, which makes the main thread record
   fills too. An AI assistant cites the clean CI run as proof that `fills` is
   race-free. Is that claim right here? What would you check first, and what
   would change your verdict? Answer format: three short sentences.
   ```cpp
   #include <thread>
   int fills = 0;
   void on_fill() { ++fills; }
   int main(int argc, char**) {
       std::thread feed([] { for (int i = 0; i < 1000; ++i) on_fill(); });
       if (argc > 1) on_fill();     // drop-copy path: production passes --dropcopy
       feed.join();
       return 0;
   }
   ```
<details><summary>Answer</summary>
Key:
  - No: the clean run is real evidence, but only for the path it ran, where one thread writes `fills`; with `--dropcopy` two threads write the plain `int` with no synchronisation, which is a data race.
  - First check whether the CI run exercised every thread that writes the shared data in the production configuration.
  - Exercise `--dropcopy` under TSan to seek a diagnostic; a clean result would not clear the code. Make the counter atomic, protect every access with the same mutex, or move the main-thread write after `feed.join()` so the writes are ordered.
</details>

### Card M7-C3 · Fix it
Q: Your CI build must catch use-after-free bugs. Fill the two blanks:
   ```sh
   clang++ -std=c++20 -O1 -g -f___=___ -fno-omit-frame-pointer my_tests.cpp
   ```
   Answer format: one line, two words separated by a comma, blanks in order.
<details><summary>Answer</summary>
Key:
  - `sanitize, address`: `-fsanitize=address` builds with AddressSanitizer,
    which reports heap-use-after-free.
</details>

### Card M7-Q5 · Recall
Q: Name three sanitizers (flag or short name) and, for each, the class of bug
   it reports. Answer format: three short lines or one line with three
   "name: bug class" pairs.
<details><summary>Answer</summary>
Key:
  - AddressSanitizer (`-fsanitize=address`): memory errors such as
    out-of-bounds access, use-after-free, double free.
  - ThreadSanitizer (`-fsanitize=thread`): data races.
  - UndefinedBehaviorSanitizer (`-fsanitize=undefined`): undefined behaviour
    such as signed integer overflow.
  - Also correct as one of the three: MemorySanitizer (`-fsanitize=memory`):
    reads of uninitialised memory (not available on macOS).
</details>

### Card M7-G4 · Spot the trap
Q: Your tests pass cleanly under ASan, yet production hits a bug caused by
   reading an uninitialised variable. (a) In one sentence, why did ASan not
   report it? (b) Which sanitizer reports such reads, and where can you run it?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) ASan checks whether memory may be accessed—in bounds and not freed—not
    whether its contents were initialised.
  - (b) MemorySanitizer (`-fsanitize=memory`), on Linux, NetBSD or FreeBSD;
    the Clang docs say it requires all program code, including libraries, to
    be instrumented. Apple clang on macOS rejects the flag.
</details>

---

### CONCEPT C4  Running sanitizers: combinations, CI, stack traces

**Why separate jobs?** One binary cannot carry every runtime checker, and a
checker cannot report code the test never executes. Separate instrumented
builds make each failure report interpretable.

**Not all at once.** ASan and TSan cannot share a build. Apple clang 21
says: "invalid argument '-fsanitize=address' not allowed with
'-fsanitize=thread'". ASan plus UBSan in one build compiled and ran.

**CI shape.** One test run built with ASan and UBSan, a second built with
TSan, both on every change. The Clang docs give typical costs of about 2x
slowdown for ASan and 5x to 15x slowdown (5x to 10x memory) for TSan, so
these are test builds, not production builds. Write tests that actually run
the threaded paths, or TSan has nothing to see.

**Stack traces.** The ASan docs: "To get nicer stack traces in error messages
add `-fno-omit-frame-pointer`." The frame pointer is a register that links
each function's stack frame to its caller's. Whether `-O2` drops it depends on
the target: in this course's test, Clang at `-O2` kept it on arm64 macOS
(`stp x29, x30` in the prologue) and dropped it for an x86-64 Linux target.
Pass the flag so the reports read the same everywhere.

**Rule:** run ASan with UBSan if useful, and run TSan in a separate CI build;
use `-fno-omit-frame-pointer` for readable reports.

### Card M7-Q6 · Apply
Q: You add both `-fsanitize=address` and `-fsanitize=thread` to one build.
   (a) What happens? (b) In one sentence, how should CI be arranged instead?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Clang refuses: "'-fsanitize=address' not allowed with
    '-fsanitize=thread'". The two cannot be combined in one build.
  - (b) Build and run the tests at least twice: one build with ASan (plus
    UBSan, which combines with it), and a separate build with TSan.
</details>

### Card M7-G5 · Spot the trap
Q: Why is `-fno-omit-frame-pointer` usually added to sanitizer builds? Answer
   in one or two sentences, including what can differ between targets.
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - The ASan documentation recommends it to get better stack traces in error
    reports ("allocated here", "freed here").
  - Whether optimised code keeps the frame pointer anyway depends on the
    target: Clang `-O2` kept it on arm64 macOS and omitted it for x86-64
    Linux in this course's test, so the flag makes reports consistent.
</details>

---

## Closing

Remember: ASan for memory errors, TSan for races, UBSan for other undefined
behaviour; ASan and TSan need separate builds; a clean run covers only what
ran. Cards you missed come back tomorrow. Next: lesson 03, benchmarks that
measure what you think they measure.
