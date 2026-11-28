# Lesson 07 — Practice Bank, part 4 of 9 (optional): What Is the Cache Line, Really

**Time: about 27 minutes (estimate)** for 3 cards (6 + 6 + 15 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: lock padding across libraries, the OS cache keys, and a report that keeps line-size sources apart.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings.

## Cards

### Card M5-PB11 · Recall
Q: You give each lock in an array its own 128-byte slot so neighbouring locks never share a line, and you write the padding by hand on the Mac: `struct Slot { std::mutex m; char pad[128 - 64]; };`. How big is `std::mutex` with libc++ on macOS and with libstdc++ on aarch64 Linux, what is `sizeof(Slot)` on each, and what declaration gives a 128-byte slot on both? First attempt: run `line_consts.cpp` (it prints `sizeof(std::mutex)`) with each toolchain you have; on reviews, recall it.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_consts.cpp -o "${TMPDIR:-/tmp}/line_consts" && "${TMPDIR:-/tmp}/line_consts"
   ```
   Answer format: the two mutex sizes, the two `sizeof(Slot)` values, then the portable declaration and its check.
<details><summary>Answer</summary>
Key:

  - `sizeof(std::mutex)` is 64 with libc++ on macOS and 48 with libstdc++ on aarch64 Linux (GCC 13.3).
  - So the hand-computed `Slot` is 128 bytes on the Mac but 112 on Linux: consecutive slots there start 112 bytes apart, and some neighbouring locks share a 128-byte line again. A padding calculation written for one library does not hold for another.
  - Portable: let the compiler pad, `struct alignas(128) Slot { std::mutex m; };` (better, with your own named line constant), which gives `sizeof(Slot)` 128 on both; add `static_assert(sizeof(Slot) % 128 == 0 && alignof(Slot) == 128)` so a later change cannot shrink it.
  - Holds for / re-check: Apple clang 21.0.0 / libc++ on M1 Max macOS; GCC 13.3 / libstdc++ in an aarch64 container on the same Mac. Library sizes can change with versions and targets; re-check with `line_consts.cpp` and `sizeof` on each toolchain.
</details>

### Card M5-PB12 · Spot the trap
Q: Before a "same cluster vs different cluster" experiment you must know the cache layout. On macOS query the keys below (with `sysctl`, or `sysctlbyname` from a program). In a Linux VM on the same machine read `/sys/devices/system/cpu/cpu0/cache/index*/shared_cpu_list` and `lscpu`. Do the two views agree?
   ```sh
   sysctl hw.perflevel0.physicalcpu hw.perflevel0.cpusperl2 hw.perflevel0.l2cachesize hw.perflevel1.physicalcpu hw.perflevel1.cpusperl2 hw.perflevel1.l2cachesize hw.cachelinesize
   ```
   Answer format: the macOS layout, the VM's report, then what you may and may not conclude.
<details><summary>Answer</summary>
Key:

  - macOS (M1 Max): 8 performance CPUs with 4 per L2 (two P-clusters, 12 MiB L2 each), 2 efficiency CPUs sharing a 4 MiB L2, 128-byte cache line.
  - The Linux VM (Docker Desktop, kernel `7.0.12-linuxkit`) reports all 10 CPUs sharing every cache level it exposes (`shared_cpu_list` 0-9 for index0-2), one cluster of 10 cores, and no cache `size` files.
  - These are guest reports: a guest-visible sharing mask is not proof about which physical cores share which cache. So these VM reports alone cannot be used to set up or validate "same cluster vs different cluster" placement. Read the topology on the host you measure, and say in results whether it came from bare metal or a VM.
  - Holds for / re-check: M1 Max host (macOS 27) and the Docker Linux VM on it. Values differ on every chip; the VM view depends on the hypervisor and its configuration. Re-run the probe on each machine.
</details>

### Card M5-PB26 · Apply
Q: A colleague's hardware-report tool has one "cache line size" line, and you must make it honest. `line_report.cpp` supplies three fixture machines (not this Mac): F1 host OS report 64, guest OS report 128, compiled recommendation 128 destructive / 64 constructive; F2 no host report, guest 64, and a library without `__cpp_lib_hardware_interference_size`; F3 no OS report at all, recommendation 256 / 64. Its draft `render()` passes the six supplied positive checks. (1) State the report contract in one line per rule. (2) Write negative tests in `negative_tests()` that fail on the draft: a missing value must not be printed as 0, and the recommendation must not appear as an observed cache line. (3) Fix `render()` so every check passes. On reviews, write the contract and the two tests from memory.
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_report.cpp -o "${TMPDIR:-/tmp}/line_report" && "${TMPDIR:-/tmp}/line_report"
   ```
   Answer format: the contract rules, your two negative tests (what each asserts and on which fixture), what the draft printed for them, and the exit code after your fix.
<details><summary>Answer</summary>
Key:

  - Contract: four separate fields, each named for its source class: `host_os_line` (host OS report), `guest_os_line` (guest OS report), `recommended_destructive` and `recommended_constructive` (compile-time recommendation). A missing value prints `unavailable`, never 0. No field takes its value from another source, and no line calls the recommendation an observed or measured cache line.
  - Missing is not zero (F2): `host_os_line=unavailable`, `recommended_destructive=unavailable`, `recommended_constructive=unavailable`, and no line ends in `=0`. The draft printed `=0` for all three (it used `value_or(0)`), so these fail on the draft.
  - Not relabeled (F1 and F3): no line named `observed_cache_line` (or any reporter field) carries the recommendation; in F3 both OS fields are `unavailable` although a recommendation exists. The draft printed `observed_cache_line=256` for F3 and `observed_cache_line=128` for F1, where it matched the guest report by coincidence, so a positive check alone did not catch it.
  - Fix: one helper that prints `name=<n>` or `name=unavailable` from a `std::optional`, called once per field; delete the `observed_cache_line` line. After the fix the program prints 13 PASS and exits 0; with the tests but the draft `render()`, every negative check fails and it exits 1; with no negative tests it exits 2.
  - Accept any equivalent contract, field names or test style if both negative tests fail on the draft and pass after the fix. Not required: this Mac's own numbers (card M5-PB7 covers those).
  - Holds for / re-check: the contract and tests are plain C++20 with no hardware access, so they hold on any toolchain; checked with Apple clang 21 on macOS arm64 and GCC 13.3 / clang 18.1.3 in an aarch64 Linux container (0 warnings). Real probes may return values these fixtures do not cover (for example `getconf` printing 0 for "unknown"): treat that as unavailable too.
</details>

---

## Close

Next: part 5, `lesson-07-practice-bank-05-memory-traffic.md` (about 27 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
