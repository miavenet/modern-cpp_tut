# Lesson 06 — Practice Bank, part 6 of 11 (optional): Build Settings and Linking

**Time: about 26 minutes (estimate)** for 4 cards (5 + 8 + 8 + 5 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: which library you compile against, `assert` under `NDEBUG`, symbol visibility and interposition.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts and listings are steadier than timings, which vary with load.

## Cards

### Card M7-PB4 · Apply
Q: You read about a new C++26 container and want to try it on your Mac today. Write the two-line check that tells you (1) which standard library you are compiling against, and (2) whether that library ships the C++26 hive container. What does it print here? Compare with:
   ```sh
   clang++ -std=c++2c -O2 modules/07-architecture/lab/practice/hive_check.cpp -o "${TMPDIR:-/tmp}/hive_check" && "${TMPDIR:-/tmp}/hive_check"
   ```
   Answer format: the two macros to test, then what this library prints.
<details><summary>Answer</summary>
Key:

  - Check the library macro: `_LIBCPP_VERSION` (libc++) or `__GLIBCXX__` (libstdc++). Here: libc++ 220106.
  - Check the feature-test macro from `<version>`: `__cpp_lib_hive`. The C++26 working draft lists it (value 202502L); the C++23 draft has no hive at all.
  - Here, with -std=c++2c, `__cpp_lib_hive` is not defined and `__has_include(<hive>)` is 0, so this library does not ship it. You would need a third-party implementation, and then your result is about that implementation, not "std".
  - Holds for / re-check: Apple clang 21 / libc++ 220106. On Ubuntu 24.04 on arm64 in a virtual machine (GCC 13.3 / clang 18.1.3, libstdc++/glibc), libstdc++ (`__GLIBCXX__` 20240904) has no `<hive>` either; GCC 13 rejects `-std=c++2c`, and with `-std=c++23` it printed the same. Expires when the library ships `<hive>`. Re-check by recompiling the same program after each toolchain update.
</details>

### Card M7-PB34 · Apply
Q: (Linux only: run it on Linux or in a Linux container.) A plugin defines two C-linkage functions, `public_value` and `internal_value`. The script builds the same source three ways: default visibility, hidden visibility for everything (`-fvisibility=hidden`), and hidden visibility with a default-visibility attribute only on `public_value`. Before running, predict which names a loader can find with `dlsym` in each build:
   ```sh
   bash modules/07-architecture/lab/practice/export_check.sh
   ```
   Answer format: public and internal found (1/0) for each build, what `nm` lists, then the habit.
<details><summary>Answer</summary>
Key:

  - Default: public=1, internal=1. All hidden: public=0, internal=0. Explicit API export: public=1, internal=0.
  - `nm`'s dynamic defined-symbol listing agrees: two names, no names, then only `public_value`. All three libraries load successfully.
  - A successful library build and `dlopen` do not certify that a required entry point is still discoverable. Test the exported API as part of a visibility change.
  - Keep the intended API explicitly exported. Export counts do not measure call overhead or justify disabling executable hardening.
  - Holds for / re-check: Ubuntu 24.04 aarch64 container, GCC 13.3, ELF shared libraries, -O2 -fPIC (exact flags in `export_check.sh`). No x86 run or timing. Mach-O and Windows export rules are outside this probe; rerun with the deployed loader and inspect its actual export table.
</details>

### Card M7-PB35 · Apply
Q: (Linux only: run it on Linux or in a Linux container.) A plugin exports `adjust(){return 11;}` and `answer(){return adjust()+1;}`. A preload library exports an alternative `adjust` returning 100. The script builds the plugin normally and with `-Wl,-Bsymbolic-functions`, which requests local binding of function references. Predict `answer` with and without the preload (`LD_PRELOAD`) for both builds under GCC and Clang at -O2 -fPIC, then check with `nm` whether both plugin functions stay exported. The script also tests GCC with `-fno-semantic-interposition` and GCC at -O0:
   ```sh
   bash modules/07-architecture/lab/practice/interpose_check.sh
   ```
   Answer format: `answer` per compiler, build and preload, what `nm` shows, then the habit.
<details><summary>Answer</summary>
Key:

  - With GCC 13.3 at -O2 -fPIC, both builds return 12 without the preload; with the preload the normal build returns 101 and the locally bound build returns 12.
  - Clang 18.1.3 at -O2 -fPIC returns 12 in both builds, with or without the preload. GCC with `-fno-semantic-interposition` also returns 12 with or without the preload; GCC's -O0 build returns 101 with the preload.
  - GCC's default `-fsemantic-interposition` keeps the call interposable here. Clang (and GCC with `-fno-semantic-interposition`) may inline it: this probe's `answer` reduces to returning 12, so optimization alone bypasses the hook.
  - Both builds still export `adjust` and `answer`, as `nm` confirms. Export visibility alone does not establish which definition an internal call selects.
  - Interposition means replacing a reference's selected definition with another definition. A binding change can disable that replacement even while the exported names remain.
  - Keep a replacement-hook test when changing compiler or linker settings. Matching outputs without the hook miss this behaviour change; no speed claim follows from it.
  - Holds for / re-check: Ubuntu 24.04 aarch64 container, GCC 13.3, Clang 18.1.3 and GNU ELF linking, ordinary non-privileged processes; these are observed compiler- and flag-dependent results. `LD_PRELOAD` is set only for each child command. Different loaders and linkers need new tests; no x86 timing, ASLR or security claim.
</details>

### Card M7-PB36 · Spot the trap
Q: A configuration loader calls `assert(commit());`. `commit()` increments a counter and returns true. Predict the counter with and without `-DNDEBUG`. Then move the call before the assertion: `const bool ok = commit(); assert(ok); (void)ok;`. Predict again. Write both versions before opening the answer, then run both builds:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/assert_effect.cpp -o "${TMPDIR:-/tmp}/assert_effect" && "${TMPDIR:-/tmp}/assert_effect"
   clang++ -std=c++20 -O2 -Wall -Wextra -DNDEBUG modules/07-architecture/lab/practice/assert_effect.cpp -o "${TMPDIR:-/tmp}/assert_effect_release" && "${TMPDIR:-/tmp}/assert_effect_release"
   ```
   Answer format: the counter for each version in each build, then where required work belongs.
<details><summary>Answer</summary>
Key:

  - Embedded call: counter 1 without `NDEBUG`, 0 with it. Separate call: counter 1 in both builds.
  - In these builds, disabling assertions also removes the expression's side effect. A passing debug test therefore misses a release-build functional change.
  - Perform required work outside `assert`. An assertion can check the result; if failure needs handling in release builds, implement that handling explicitly as well.
  - Holds for / re-check: Apple clang 21 with libc++ (macOS arm64) and GCC 13.3 with libstdc++ (Ubuntu 24.04 aarch64), C++20, `-O2 -Wall -Wextra`, with and without `-DNDEBUG`. The assertion facility permits disabling checks ([assertions.general], [cassert.syn], [assertions.assert]). This is an assertion macro test, not a statement about C++ contracts or every library hardening mode. Re-run both builds when replacing the check mechanism.
</details>

---

## Close

Next: part 7, `lesson-06-practice-bank-07-vectorize.md` (about 28 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
