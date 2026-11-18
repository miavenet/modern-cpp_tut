# Module 07 — Architecture: Modules, Sanitizers, Benchmarks, CMake

**What this module adds:** the tools around the code. C++20 named modules
built by hand, sanitizers that turn undefined behaviour into reports,
benchmarks whose loops you have checked in the assembly (plus C++23
library and language features), and target-based CMake with a sanitizer
build type.

**What it assumes:** modules 01 to 03 and compiling from a terminal. Tested
with Apple clang 21, libc++ and CMake 4.3.3 on macOS arm64. Where support
differs (named modules need `-fcxx-modules` on Apple clang; CMake cannot yet
scan modules with AppleClang; MemorySanitizer is not available on macOS;
contracts are not implemented), the lesson says so next to the sample.

## Subject & scope
- **Subject:** module interface and implementation units, `export`,
  `import`, global module fragment, macro isolation, visible versus reachable
  names, current tool support; ASan, TSan, UBSan, MSan and their limits,
  sanitizer combinations, frame pointers; Google Benchmark v1.9.5,
  `DoNotOptimize`, `ClobberMemory`, a library-free harness, reading the
  assembly; `std::flat_map`, `std::expected`, deducing `this` (C++23), a
  note on C++26 contracts; target-based CMake, PRIVATE/PUBLIC/INTERFACE,
  generator expressions, custom build types, `cmake_minimum_required` in
  CMake 4.
- **Out of scope:** module partitions, header units, `import std`,
  `std::mdspan`, reflection, clang-tidy and cppcheck (named in earlier
  versions without a card or a test).

## Time cost (honest)
- **First pass:** about 175 minutes over 5 lessons (35 + 30 + 45 + 40 + 25),
  38 cards (5 pretests, 5 completions, 28 recall/apply/gotcha/synthesis).
  That includes the module build, sanitizer runs and CMake lab. The optional
  Google Benchmark download depends on the network and is not included.
- **Reviews:** cards come back on the driver's schedule (see the README); a
  missed card returns the next day.
- **Interruptible:** stop after any card; the driver resumes where you left off.

## Learning objectives
1. Given a small header-based component, the learner can write it as a named
   module, build and import it with the two-step Clang commands, explain macro
   isolation and reachability, and state what this toolchain supports.
2. Given a bug class, the learner can choose and run the matching sanitizer,
   read its report, and explain why a clean run is evidence rather than proof.
3. Given a micro-benchmark, the learner can keep measured work in its loop,
   confirm that in assembly, and choose the covered C++23 features where they fit.
4. Given the two-target CMake lab, the learner can fix a scope error and add
   per-configuration flags and a sanitizer build type.
5. LPA lens on benchmarks (lesson 05): compute a nearest-rank tail
   percentile from samples, say what a median of means hides, and choose
   `flat_map` or `map` by the operations on the latency path.

## Ways to study

tiers: [T0, T1, T2, T3]

The offline driver runs the cards and their checks; the lessons also work as
plain Markdown with folded answers. An optional AI tutor can use PROTOCOL.md.

## Labs and runnable samples

The `lab/` folder contains module builds, sanitizer samples, benchmarks and
the two-target CMake project.
Lesson 05 runs `lab/bench/map_tail.cpp` (build line on its fourth line), which
uses the shared histogram `../06-hft-patterns/lab/measure/lat_hist.hpp`.

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
