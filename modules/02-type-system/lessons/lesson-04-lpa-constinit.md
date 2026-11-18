# Lesson 04 — LPA Lens: No First-Use Work on the Hot Path (`constinit`)

**Time: about 14 minutes** (reading about 2, the lab about 8, cards about 4). Stop after any card; the driver resumes there. Do it after lesson 01.

**Hook:** a function-local `static` with a run-time initialiser looks free. Its first caller pays for the whole initialiser inside one request, and every later caller pays a hidden flag check.

**You will be able to:** say what that guard costs on the first and on every call, and declare a `constinit` variable that removes both.

**Needs from earlier:** lesson 01 (concept C5, `constinit` and `consteval`).

## Pretest — commit your guess FIRST

### Card M2-LPA3 · Pretest
Q: Guess, before you read concept C8. Two C++20 counters. A: inside a function, `static std::uint64_t seq = start_value();` (`start_value` is an ordinary function, not `constexpr`). B: at namespace scope, `constinit thread_local std::uint64_t seq = 0;`. Compile each with `-O2` to assembly. How many calls to the hidden one-time-initialisation guard (`__cxa_guard_acquire`) does each contain, and which one do all threads share?
   Answer format: two numbers and one letter. A guess is fine.

<details><summary>Answer</summary>
Key:
  - A: 1 guard call; B: 0 guard calls (assembly counted, `-O2`; there is no timing claim). A is the one all threads share. B is `thread_local`, so each thread has its own copy.
  - A's guard exists because the first call must run `start_value()` exactly once, even if two threads arrive together. `constinit` promises the value is in the binary before `main`, so B needs no guard.
</details>

---

---

## The mechanism

### CONCEPT C8  LPA lens: no first-use work on the hot path

The *Latency Prioritized Approach* (LPA, used across this course) judges a hot path by its far tail.

- **Default does / LPA does:** the default writes `static Table t = load();`
  inside the function that needs it, so the first call builds it. LPA makes
  the starting value a constant expression and says so with `constinit`
  (or `static constexpr` for a read-only table).
- **Why (mechanism):** a function-local static with a run-time initialiser
  gets a hidden guard: every call checks a flag, and the *first* call takes
  a lock (`__cxa_guard_acquire`) and runs the whole initialiser, inside
  whatever request happened to come first. Compiled with `-O2`, the run-time
  version contained one `__cxa_guard_acquire` call; the constant `static
  constexpr` local and the `constinit` global contained none (assembly
  counted; there is no timing claim here).
- **Gives up:** the value must be computable at compile time: no reading a
  config file or the environment into it. Load those at start-up into a
  `constinit` variable instead.

### Card M2-LPA1 · Recall
Q: A pricing function starts with `static auto table = build_table(read_config());`.
   What runs on the *first* call that does not run on later calls, and what
   does the hot path still pay on every call?
   Answer format: one line; separate the two parts with a semicolon.

<details><summary>Answer</summary>
Key:
  - First call: the guard lock (`__cxa_guard_acquire`) plus the whole
    initialiser, here a config read and a table build, inside one unlucky
    request; that is one huge sample in the tail.
  - Every call: a check of the guard flag (a load and a branch). With a
    constant initialiser declared `constinit` or `static constexpr`, both
    disappear: the value is in the binary before `main` starts.
</details>

### Card M2-LPA2 · Apply
Q: Every order on the hot path takes the next number from a per-thread
   sequence counter that starts at 0. Write its declaration so the compiler
   *guarantees* there is no run-time initialisation (and refuses to build
   otherwise). Name it `seq`, type `std::uint64_t`.
   Answer format: one C++ declaration on one line.

<details><summary>Answer</summary>
Key:
  - `constinit thread_local std::uint64_t seq = 0;` (the specifiers may come in
    any order, with `inline` or `static`, and `{0}` or `(0)` for `= 0`; each
    may appear only once). `constinit` makes a non-constant
    initialiser a build error; the variable stays writable, so `++seq` works.
  - The check compiles your line and runs two threads: it needs `constinit`, a writable
    `std::uint64_t` named `seq`, one copy per thread, starting at 0.
  - `constexpr` would make it read-only, and plain `thread_local` compiles but
    promises nothing: with an initialiser the compiler cannot see, every
    access may go through an initialisation wrapper call.
</details>

### Card M2-LPA4 · Build and check
Q: Do the lab: fix `modules/02-type-system/lab/work/constinit_guard.cpp` so each thread has its own counter and no run-time initialisation runs. Run it, then submit that path.

   Step 0 (from the tutorial root; the `cp` OVERWRITES an earlier work copy):
   ```sh
   mkdir -p modules/02-type-system/lab/work && cp modules/02-type-system/lab/starter/constinit_guard.cpp modules/02-type-system/lab/work/
   clang++ -std=c++20 -Wall -Wextra -O2 modules/02-type-system/lab/work/constinit_guard.cpp -o "${TMPDIR:-/tmp}/constinit_guard" && "${TMPDIR:-/tmp}/constinit_guard"
   clang++ -std=c++20 -O2 -S -o - modules/02-type-system/lab/work/constinit_guard.cpp | grep -c __cxa_guard_acquire
   ```
   The starter prints `RESULT: TODO`. **Predict** first: what number does thread B start counting from, and what does the `grep -c` print? Then do the TODO (one declaration at namespace scope, delete the function-local static), rebuild, and run all three lines again. Check with
   `printf '%s\n' modules/02-type-system/lab/work/constinit_guard.cpp | modules/02-type-system/checks/constinit_lab.check.sh`.
   It grades structure and counters, never a time.
Hint: it is the same declaration as card M2-LPA2.

<details><summary>Answer</summary>
Key:

  - Delete the `static` line in `next_id()` and put `constinit thread_local std::uint64_t seq = 0;` above it. `next_id()` is then just `return ++seq;`.
  - Before: thread B starts at 4 (the counter is shared), `start_value()` ran once, and `grep -c` prints 1. After: both threads count 1 to 3, the initialiser-call counter is 0, `grep -c` prints 0, and the program prints `RESULT: PASS`.
  - The check also refuses a shared `constinit` counter (threads must restart at 1), a `thread_local` that still calls `start_value()`, and a file that keeps a guarded function-local static.
</details>

---

## Close

**Remember:** a constant initialiser (`constinit`, or `static constexpr` for read-only data) puts the value in the binary before `main`: no guard call, no first-call stall. **Tomorrow:** missed cards come back. **Next:** lesson 02, concepts.
