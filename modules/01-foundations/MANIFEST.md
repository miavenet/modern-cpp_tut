# Module 01 — Foundations: move semantics, smart pointers, `auto`, lambdas

This module covers the C++11/14 features a C++98 programmer meets first:
moving instead of copying (lesson 01), ownership in the type (lesson 02),
`auto` and lambdas (lesson 03), and five everyday replacements for C++98 habits
(lesson 04). Lesson 01 also holds the course primer: what changed after C++98,
how a lesson works, and how to compile the samples.

## Subject & scope
- **Subject:** rvalue references and move constructors, copy elision and
  implicit move, moved-from state; `unique_ptr`, `shared_ptr`, `weak_ptr`;
  `auto` deduction and range-for; lambdas, captures and their lifetimes;
  `enum class`, `<chrono>`, `std::array`, brace initialisation, `thread_local`.
- **Grain:** predict which constructor runs, what a variable's type is, or
  what a capture holds; write the short fix; explain why.
- **Prerequisites:** C++98 classes, copy constructors, templates and pointers.
  Samples were compiled with Apple clang 21 (`clang++ -std=c++20 -Wall -Wextra`,
  or the standard a sample names) on macOS arm64 with libc++; other toolchains
  are untested.

## Time cost
- **First pass:** about 169 minutes over five lessons (34 + 29 + 45 + 35 + 26 min):
  7 pretests, 6 completion cards, 32 recall, apply, gotcha and synthesis
  cards and one lab (46 in all). At about 20 minutes a sitting that is about nine sittings.
- **Optional practice bank:** lesson 06 (`lesson-06-practice-bank.md`), about 30 minutes,
  4 self-graded cards on measurement, research and C++ techniques and on how
  results expire. It is optional and not counted in the first pass above; its
  runnable programs are in `lab/practice/`.
- **Reviews:** a missed card returns the next day; after hits a card returns
  about 2, 9, 30, 51 and 72 days after the first attempt when hits continue. A review sitting takes a few minutes per due card.
- **Interruptible:** yes; progress is saved after every card.

## Learning objectives
1. Given a line that initialises, passes or returns an object, the learner can
   say whether the copy constructor, the move constructor or neither runs, and
   write a `noexcept` move constructor.
2. Given C++98 `new`/`delete` code, the learner can rewrite it with
   `unique_ptr` (or `shared_ptr`/`weak_ptr` where ownership is shared or
   observed) and explain what happens to a moved-from `unique_ptr`.
3. Given an `auto` declaration or range-for loop, the learner can name the
   deduced type and choose between `auto`, `auto&` and `const auto&`.
4. Given a C++98 functor or callback, the learner can write the lambda and
   choose captures that stay valid when it runs later.
5. Given C++98 code using plain enums, integer timeouts, C arrays or shared
   counters, the learner can name the C++11 replacement and the trap it keeps.
6. LPA lens on growth (lesson 05 lab): read a `push_back` latency histogram
   for its tail, explain why `noexcept` turns growth copies into moves and why
   `reserve` plus first touch removes growth and page faults from the timed
   path, and name what that gives up.

## Lab

Lesson 05: `lab/starter/push_tail.cpp` (prints `RESULT: TODO`) and
`lab/solution/push_tail.cpp`; copy the starter to `lab/work/` (gitignored). It
includes the shared histogram `../06-hft-patterns/lab/measure/lat_hist.hpp`.

## Ways to study
```
tiers: [T0, T1, T2, T3]
detect:
  - model + tools -> T3   (an AI tutor; see PROTOCOL.md, optional)
  - model, no tools -> T2 (an AI grades free text against the Key)
  - python3 drive.py -> T1 (the offline driver: checks + self-grading)
  - else -> T0            (paper: read lessons/*.md, answer, then open the Key)
```
In plain words: the offline driver (`./start.sh`, or
`python3 ../../drive.py lessons --progress progress.json --today $(date +%F)`
from this folder) runs a compile check for completion cards M1-C1, M1-C3,
M1-C5 and M1-C6; M1-C3 checks type names without compiling. You grade
yourself against the key for the rest.

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
