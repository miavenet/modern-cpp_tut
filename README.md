# Modern C++ for low latency systems

Learn the C++ features used in ownership, concurrency, memory layout and low
latency code by predicting, editing, checking and recalling. The full first
pass is **about 18.8 hours over about 11 weeks** at five 20-minute sittings a
week. Reviews add short later visits. Stop after any card and resume where you
left off.

## Quick start

```sh
./setup.sh
./start.sh
```

These commands use Python's standard library. The driver needs no AI model or
network. C++ checks need `clang++`; one build lesson uses `cmake`.

## One sitting

Read the teaching shown before each card, then answer from memory on one line.
For several blanks, type values in order separated by commas. A check reports
`HIT` (passed), `MISS` (something to fix), or `SETUP PROBLEM` (the check could
not run). After a miss, edit and type `retry`, or type `giveup` to see the key.
The first attempt sets the review date. Open-ended answers show the key and
ask for an honest self-grade. `:hint` gives a clue where offered; `:status`
shows your place; `:skip` records “don't know”; `:quit` pauses. `:help` lists
all commands. Progress saves after each card.

A sitting stops after 12 cards or about 20 minutes. Up to four due reviews
come first, then at most six new recurring cards. Lesson 01 takes about 34
minutes, so expect two
sittings. On a later day, run `./start.sh` again. With uninterrupted hits,
reviews fall about 2, 9, 30, 51 and 72 days after the first attempt; misses
and partial answers return the next day.

## First-pass time

The minutes below are estimates from the lesson headers, including reading,
cards and the listed hands-on work. A sitting is capped at 20 minutes; the
counts round each module up separately.

| Module | Lessons | First pass | 20-minute sittings |
|---|---:|---:|---:|
| 01 Foundations | 5 | about 169 min | about 9 |
| 02 Type system | 4 | about 119 min | about 6 |
| 03 Concurrency | 5 | about 151 min | about 8 |
| 04 Modern library | 5 | about 149 min | about 8 |
| 05 Memory and performance | 6 | about 184 min | about 10 |
| 06 Low latency patterns | 5 | about 183 min | about 10 |
| 07 Architecture and tools | 5 | about 175 min | about 9 |

## Short path

For a first pass under a tighter clock, do the **core** lessons below first
(about 12.5 hours), then return to the optional depth. Keep the listed order
within each module. Later modules use ownership, atomics, views and the pool
lab; their lessons are core here.

| Module | Core lessons and time | Optional depth and time |
|---|---|---|
| 01 | 01–05: about 169 min | — |
| 02 | 01, 02, 04 (constinit LPA lens): about 78 min | 03 traits and folds: about 41 min |
| 03 | 01–03: about 85 min | 04 coroutines: about 39 min; 05 spin vs sleep at the tail: about 27 min |
| 04 | 01–05: about 149 min | — |
| 05 | 01, 02, 05, 06: about 131 min | 03 compile-time work, 04 parallel algorithms: about 53 min |
| 06 | 01–02: about 70 min | 03 measurement, 04 prefetch, 05 tail-budget capstone: about 113 min |
| 07 | 02 and 04: about 70 min | 01 modules, 03 benchmarking, 05 benchmarks as distributions: about 105 min |

Optional practice banks, not counted above, are the last lessons of modules 01 to 07 (about 95, 88, 188, 50, 230, 293 and 296 min in all), split into themed parts of at most 30 min each (4, 4, 8, 2, 9, 12 and 11 parts, named `lesson-NN-practice-bank-KK-<theme>.md`); the driver offers their cards after the module's other lessons. Bank minutes are authors' estimates for a learner with the tools ready, not measured first-attempt times. The driver does not record durations, so recalibrate them yourself: note the clock time when you start and finish a part, count tool setup separately from card work, and if a part runs well over its estimate, stop after any card and finish it in a later sitting. `:skip` on a card records “don't know”, so it returns the next day; `:quit` when a bank card appears to leave the bank for later.

## Other ways to use it

- Choose a module: `python3 launch.py --module 01 --tier T1` (`T1` is the offline driver).
- Generate separate paper questions, answers and a review ledger: `python3 launch.py --paper`. The lesson files also work as plain Markdown.
- See progress: `python3 launch.py --status`. After studying at least 12 cards,
  try a delayed mixed check with `python3 launch.py --exam`. Start over: `./wipe.sh`.
- Diagnose what to read: `python3 drive.py modules --today $(date +%F) --diagnose`
  asks 10 one-line questions spread over every module (about 2 min each; `--set N`
  changes the count; pass one module to stay inside it), then lists the lesson
  sections to read, worst first. It is read-only: no progress file is written.
- Run every tooling test: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools -p 'test_*.py'`.
- Run the structural audit for all modules: `for module in modules/*; do PYTHONDONTWRITEBYTECODE=1 python3 evals/audit.py "$module"; done`.

[PROTOCOL.md](PROTOCOL.md) is optional instructions for an AI tutor.
[GOTCHAS.md](GOTCHAS.md) and [CONCEPT-MAP.md](CONCEPT-MAP.md) are compact
reference pages. Each chapter uses `### Card ID · Label` headings; the module’s
`cards.json` holds grading, sources, objectives and time budgets.

For this release every complete lesson sample and lab was compiled and run on
the toolchain below, and the lessons were reviewed against the C++ working
draft and the sources of their stories. Other compilers and platforms are
untested.

## Toolchain

The course was exercised with Apple clang 21 and libc++ on macOS arm64, Python
3.14 and CMake 4.3. Other versions and platforms are untested. The driver
requires Python 3.9 or later. `./setup.sh` checks required executables and
probes optional library features. Some examples require C++23 library support;
x86 intrinsics need x86; a ThreadSanitizer run depends on toolchain support.
The relevant lessons give their build commands and feature limits.

Google Benchmark is optional for module 07. `./setup.sh --benchmark` downloads
and builds pinned v1.9.5 under ignored `third_party/`. Its upstream
[license](https://github.com/google/benchmark/blob/v1.9.5/LICENSE) is Apache 2.0.
