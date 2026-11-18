# Lesson 06 — Practice Bank (optional): Clocks and Passing by Value

**Time: about 30 minutes** for 4 cards (8 + 12 + 5 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Hook: a function that takes 15 ns, and a per-call log that says it took 0 ns most of the time.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Timings come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) and vary with load; counts and listings are steadier.

## Cards

### Card M1-PB1 · Apply
Q: A teammate says: "`steady_clock` counts in nanoseconds, so I will time every call of our short pricing function with two `now()` calls and log each result." The function takes a few tens of ns at most. List two things that make the logged numbers wrong, and the measurement that exposes each.
   Answer format: two short points, each with the measurement that exposes it.
<details><summary>Answer</summary>
Key:

  - **Resolution.** Here the clock advances in 41.67 ns ticks. A function shorter than a tick mostly reads as 0. Measured with a function that takes about 15 ns per call: over 200,000 single-shot timings, more than half read 0 ns in every run (the exact share changed from run to run with machine load, so it is not given here) and almost all the rest read 41 or 42 ns. No sample is close to the true cost. Expose it: print a histogram of the single-shot values.
  - **Read overhead.** Each `now()` costs about 20 ns here (the program's `wall/read` line), more than the function itself. Expose it: time a batch of back-to-back empty `now()` calls and divide by the count (a single empty pair also mostly reads 0 here). The `wall/read` loop also converts and stores each value, so it slightly overstates the bare read.
  - Remedy for both: time a batch. One measurement around 20 million calls gave about 15 ns per call in every run (one machine). Compare the single-shot histogram with the batched mean; if they disagree, trust the batch for cost.
  - Accept other valid points if each comes with a measurement that exposes it.
  - Holds for / re-check: the tick size and the 15 ns function are this Mac's; the method (histogram of single shots, compare with a batch) works for any clock and any short function. Re-check the tick on each new Mac: `clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/clock_res.cpp -o "${TMPDIR:-/tmp}/clock_res" && "${TMPDIR:-/tmp}/clock_res"` prints the single-shot values and the batched cost. `clock_res.cpp` builds on macOS only (it includes `<mach/mach_time.h>`); on another OS, port its `mach_timebase_info` lines to that OS's clock API, or apply the same method (single-shot histogram, then a batch) with your own timer.
</details>

### Card M1-PB2 · Apply
Q: An 8-byte handle `struct P { long h; };` is passed by value to a function in another file. Four variants add one thing each: `D` a user-written destructor `~D();`, `DF` a destructor `= default` in the class, `OD` a copy constructor declared in the class and defaulted out of line (`OD::OD(const OD&) = default;`), `DL` deleted copy and move constructors. Predict which variants travel in a register and which are stored to the caller's stack and passed by address; for `DL`, say what you expect and why. Then compile for arm64 and x86-64 and read the call sites:
   ```sh
   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=arm64-apple-macos
   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=x86_64-linux-gnu
   ```
   Answer format: list the register variants and the stack variants, say where you expect `DL` and why, and give the rule in one line.
<details><summary>Answer</summary>
Key:

  - Register, plain tail jump: `P` and `DF` (arm64 `b __Z5sinkP1P`; x86-64 `jmp _Z5sinkP1P@PLT`).
  - Stack and address: `D` and `OD`. arm64, both: `str x0, [sp, #8]` / `add x0, sp, #8`. x86-64 `D`: `movq %rdi, 8(%rsp)` / `leaq 8(%rsp), %rdi`; x86-64 `OD`: `movq %rdi, (%rsp)` / `movq %rsp, %rdi`. For `D` the caller also calls `~D` after the call returns.
  - Reason: under the Itanium C++ ABI a type with a non-trivial copy constructor, move constructor or destructor is "non-trivial for the purposes of calls", and "the caller must allocate space for a temporary and pass that temporary by reference". The C++ standard permits the extra temporary that makes the register path possible only "if X has at least one eligible copy or move constructor (11.4.4), each such constructor is trivial, and the destructor of X is either trivial or deleted" ([class.temporary]/3). A deleted function is not eligible ([special]/6).
  - Why `OD` is non-trivial although it says `= default`: a function is user-provided if it is "not explicitly defaulted or deleted on its first declaration" ([dcl.fct.def.default]/5), and a user-provided copy constructor or destructor is not trivial.
  - `DL`: either placement is a hit if you give the rule behind it (stack: Itanium treats a type whose copy and move constructors are all deleted as non-trivial for calls; register: no non-trivial copy, move or destructor). This clang passed it in a register; see the Holds-for line.
  - Rule: to keep a small type in registers, remove the user-written copy constructor, move constructor and destructor, or `= default` them on their first declaration.
  - Holds for / re-check: Itanium-ABI targets (Linux, macOS), Apple clang 21, x86-64 compile-only. `DL` is an observed disagreement: it has no eligible copy or move constructor, so [class.temporary]/3 does not grant the permission, and Itanium 1.1 calls a type whose copy and move constructors are all deleted non-trivial for calls; yet this clang passed `DL` in a register on both targets (`b __Z6sinkDL2DL`, `jmp _Z6sinkDL2DL@PLT`). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 (arm64) and clang 18 (arm64 and x86-64) passed `DL` by address, so the register placement is specific to this compiler version. Do not rely on either for such types; check your compiler. Other ABIs (e.g. Windows) were not checked, and the cost of the memory trip was not measured. Re-check by compiling the same file with your compiler and target.
</details>

### Card M1-PB3 · Apply
Q: `Owner` is a move-only owning handle whose destructor prints a line. `take(Owner)` takes it by value. `main` runs one full-expression: `(take(Owner(new int(1))), std::puts("rest of the same full-expression"))`. Predict where "~Owner" prints, with and without `[[clang::trivial_abi]]` on `Owner`. Then build and run both:
   ```sh
   clang++ -std=c++20 -O2 modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/plain" && "${TMPDIR:-/tmp}/plain"
   clang++ -std=c++20 -O2 -DUSE_TRIVIAL_ABI modules/01-foundations/lab/practice/trivial_abi_order.cpp -o "${TMPDIR:-/tmp}/tabi" && "${TMPDIR:-/tmp}/tabi"
   ```
   Answer format: one line for each build.
<details><summary>Answer</summary>
Key:

  - Without the attribute (this compiler): the parameter is destroyed by the caller, after the rest of the full-expression.
  - With it: the callee destroys the parameter before returning, so "~Owner" prints before "rest of the same full-expression".
  - The clang docs also warn the compiler "may omit the call to the copy constructor", so its side effects may not happen; types that depend on their own address "should not be declared [[trivial_abi]]".
  - Holds for / re-check: clang only (a `clang::` attribute). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), GCC 13 ignored it with a warning (`scoped attribute directive ignored`) and printed "~Owner" after "rest of the same full-expression" in both builds. When a by-value parameter is destroyed without the attribute is implementation-defined in C++ ([expr.call]), so check your compiler. Re-check the clang attribute docs for the current rules.
</details>

### Card M1-PB4 · Recall
Q: On Linux you need two clocks: one to measure how long a request took (it must never jump if an admin changes the time), and one to print the time of day in a log. For each clock, say which job it fits and why: `CLOCK_REALTIME`, `CLOCK_MONOTONIC`, `CLOCK_MONOTONIC_RAW`. First attempt: look it up in the Linux `clock_gettime(2)` man page (man7.org); on reviews, recall it.
   Answer format: one line per clock.
<details><summary>Answer</summary>
Key:

  - `CLOCK_REALTIME`: time of day. It is affected by discontinuous jumps (the time being set) and by NTP frequency adjustments. Bad for intervals.
  - `CLOCK_MONOTONIC`: intervals. Not affected by jumps, but still affected by frequency adjustments.
  - `CLOCK_MONOTONIC_RAW`: intervals at the raw hardware rate; not subject to frequency adjustments.
  - Holds for / re-check: Linux, per the `clock_gettime(2)` man page. macOS has the same three names plus others (e.g. `CLOCK_UPTIME_RAW`), with different rules: its `CLOCK_MONOTONIC` keeps counting during sleep (`man 3 clock_gettime`). Read that platform's man page before reusing the choice.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
