# Module 02 — Type system: constexpr, concepts, traits

This module moves work and checks from run time to compile time:
`constexpr`/`consteval`/`constinit` (lesson 01), C++20 concepts (lesson 02),
type traits, `if constexpr` and fold expressions (lesson 03), and the LPA lens on
`constinit` (lesson 04).

## Subject & scope
- **Subject:** compile-time computation (C++11 to C++20), template
  constraints from C++11 `std::enable_if` to C++20 concepts, type traits,
  `if constexpr` and fold expressions (C++17).
- **Grain:** predict whether something happens at compile time, write a small
  concept, branch on a trait, fold a pack, and explain what compile-time checks
  cannot catch.
- **Prerequisites:** module 01; C++98 templates and overloading. Samples were
  compiled with Apple clang 21 (`clang++ -std=c++20 -Wall -Wextra`, or the
  standard a sample names) on macOS arm64; other toolchains are untested.

## Time cost
- **First pass:** about 119 minutes over four lessons (29 + 35 + 41 + 14 min):
  4 pretests, 4 completion cards, 26 recall, apply, gotcha and synthesis cards,
  and one short lab (the `constinit` lab, about 8 minutes of lesson 04).
- **Optional practice bank:** lesson 05 (`lesson-05-practice-bank.md`), about 88 minutes,
  12 self-graded cards on measurement, research and C++ techniques and on how
  results expire. It is optional and not counted in the first pass above; its
  runnable programs are in `lab/practice/`.
- **Reviews:** a missed card returns the next day; after hits a card returns
  about 2, 9, 30, 51 and 72 days after the first attempt when hits continue. A review sitting takes a few minutes per due card.
- **Interruptible:** yes; progress is saved after every card.

## Learning objectives
1. Given a `constexpr` call, the learner can say whether compile-time
   evaluation is guaranteed (only where a constant expression is required, or
   with `consteval`), and fix a call so that it is.
2. Given a C++11 `std::enable_if` template, the learner can write the C++20
   concept or `requires` version and explain what the diagnostic gains.
3. Given two constrained overloads, the learner can predict which one is
   chosen, or explain why the call is ambiguous (subsumption).
4. Given a template that must treat types differently, the learner can use a
   type trait with `if constexpr` and predict which branch is compiled.
5. Given a parameter pack, the learner can write a fold expression and handle
   the empty pack.
6. The learner can identify what concepts, `consteval` and folds check at
   compile time, and what still needs run-time validation.
7. LPA lens on initialisation: say what a function-local static with a
   run-time initialiser costs on its first and every call, and declare a
   `constinit` variable that removes both, then fix a small program so each
   thread has its own constant-initialised counter (lesson 04, M2-LPA4).

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
from this folder) runs automated checks for completion cards M2-C1, M2-C2,
M2-C3 and M2-C4, for the apply card M2-LPA2 (it compiles and runs your declaration) and for the lab card M2-LPA4 (`lab/starter/constinit_guard.cpp`, `lab/solution/constinit_guard.cpp`; work in `lab/work/`), and asks you to grade yourself against the key for the rest.

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
