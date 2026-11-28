# Lesson 06 — Practice Bank, part 1 of 12 (optional): Pages and Page Faults

**Time: about 28 minutes (estimate)** for 3 cards (4 + 12 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: first-touch fault budgets, guard pages, and prefaulting at start-up.

Hook: A guard-page recipe meets a different operating system.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB1 · Recall
Q: A teammate on Linux worked out how many first-touch page faults a 1-million-element array of 48-byte records costs, and built a start-up budget on it. You port the code to a Mac. Which one number do you check first, what is it on Apple Silicon macOS (check with `getconf PAGESIZE`), and how does the count change?
   Answer format: the number, its value here, and how the fault count scales.
<details><summary>Answer</summary>
Key:

  - The page size: faults = bytes touched / page size, if each page is first touched by the array.
  - This M1 Max reports a 16384-byte page (`getconf PAGESIZE`, and `sysconf(_SC_PAGESIZE)` in the program). 48 MB / 16384 = 2929.7, and the measured count was 2930 faults.
  - A 4 KiB-page system without huge pages needs four times as many faults for the same bytes (about 11,700 here). Huge pages need far fewer.
  - Holds for / re-check: page size depends on the OS and its configuration (and on huge-page settings on Linux), so re-check per machine: `getconf PAGESIZE` or `sysconf(_SC_PAGESIZE)`, then count faults to confirm, for example with `clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/vec_growth.cpp -o "${TMPDIR:-/tmp}/vec_growth" && "${TMPDIR:-/tmp}/vec_growth" reserve` (its `minor faults in loop`). Example: on Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), with 4 KiB pages, the reserved array took 477 faults with transparent huge pages set to `always` and 11,719 with them disabled.
</details>

### Card M6-PB4 · Apply
Q: To catch buffer overruns at once, you put an inaccessible guard region right after a buffer: map buffer + guard as one anonymous region, then `mprotect(base + bufsize, page, PROT_NONE)`. Try bufsize = 4096 and bufsize = `sysconf(_SC_PAGESIZE)`. For the size that works, a child process writes one byte past the end. Which size works, why, and how does the child end?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/guard_page.cpp -o "${TMPDIR:-/tmp}/guard_page" && "${TMPDIR:-/tmp}/guard_page"
   ```
   Answer format: the size that works and why, then how the child ends.
<details><summary>Answer</summary>
Key:

  - The page size here is 16384. With bufsize 4096, `mprotect` at `base + 4096` fails with EINVAL: the man page says EINVAL means "addr is not a multiple of the page size".
  - With bufsize = one page, `mprotect` succeeds, and the child that writes one byte past the end is killed by signal 10 (SIGBUS here, not SIGSEGV).
  - So protection works in whole pages: a guard sits on a page boundary, and buffers that need one are padded to a page multiple. Query the page size at run time; never hard-code 4096.
  - Handle both SIGSEGV and SIGBUS if you trap such faults: which one you get is platform-specific.
  - Holds for / re-check: macOS on Apple Silicon (16 KiB pages here). On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc) the page size is 4096, so both sizes worked and the child got signal 11 (SIGSEGV); other platforms were not checked. Re-check: print the page size, the `mprotect` result and the child's signal on the target.
</details>

### Card M6-PB15 · Apply
Q: `prefault.cpp` mallocs 64 MiB, writes one byte per page (the prefault: touch every page once during start-up so later passes find them mapped), then runs a "hot loop" three times over the same pages. It prints the page size, the number of pages, and the minor faults (`ru_minflt` from `getrusage`) for each phase. On Linux, the argument `nothp` first calls `prctl(PR_SET_THP_DISABLE, 1, ...)` (THP: transparent huge pages, 2 MiB here) and prints its return value and `PR_GET_THP_DISABLE`. Predict the fault counts on macOS, on Linux by default, and on Linux with `nothp`. Then run each 3 times, and list the per-size THP files under `/sys/kernel/mm/transparent_hugepage/`:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault"
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" nothp
   ```
   Answer format: one line per platform/mode, what zero hot faults does and does not mean, then the lessons.
<details><summary>Answer</summary>
Key:

  - macOS (M1 Max): page = 16384 bytes, 4096 pages, first touch = 4096 faults (one per page), hot loop = 0. Same in 3 of 3 runs.
  - Linux VM, `nothp`: `prctl` returned 0 and `PR_GET_THP_DISABLE` read back 1 (so the request took effect); page = 4096 bytes, 16384 pages, first touch = 16383 faults (about one per page), hot loop = 0, in 3 of 3 runs.
  - Linux VM, default (top-level THP mode `[always]`): first touch = 542 faults in 3 of 3 runs, far fewer than pages, because huge pages back most of the buffer; the exact count varies. Hot loop = 0.
  - These three immediate passes recorded zero additional minor faults. That is a measurement, not a lifetime guarantee: touched pages can later be reclaimed, and after `fork` they become copy-on-write so the next write faults again. mlock(2) says writes after `fork` "will cause a page fault" and that `fork` should not follow `mlock`; real fault avoidance needs a residency/lifecycle strategy (for example `mlockall` plus no `fork`), checked by counting faults.
  - Lessons: (1) do the touching at start-up, then count faults in the hot phase instead of assuming zero; (2) never assume a page size: read `sysconf(_SC_PAGESIZE)`; (3) the fault count depends on THP, so a "faults per MiB" figure from another box does not transfer; (4) check the return value of a control call such as `prctl` before labelling a run `nothp`.
  - The top-level `enabled` file alone does not describe all THP: this VM also has per-size `hugepages-*/enabled` files (all `[never]` except `hugepages-2048kB`, `[inherit]`), and the kernel THP documentation says `MADV_COLLAPSE` ignores the sysfs settings and that the process-level `prctl` control is inherited across `fork` and `execve`.
  - Holds for / re-check: macOS on M1 Max (16 KiB pages) and one Linux VM kernel (Docker Desktop on the same Mac; 4 KiB pages, top-level THP `always`). Counts change with page size, THP mode and allocator (a large `malloc` may come straight from `mmap`). Re-check with `prefault.cpp` on the target box.
</details>

---

## Close

Next: part 2, `lesson-06-practice-bank-02-clock-reads.md` (about 26 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
