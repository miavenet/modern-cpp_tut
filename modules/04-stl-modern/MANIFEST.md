# Module 04 — Modern Standard Library: Vocabulary Types, Ranges and Views

What this module adds: types that put "maybe", "one of" and "value or
error" into the signature (`optional`, `variant`, `expected`), C++20 range
algorithms and lazy views, structured bindings and if-init, `std::format`,
and non-owning views of text and buffers (`string_view`, `span`). It
assumes module 01 and a C++20 compiler (C++23 for `expected` and `print`);
it was tested with Apple clang 21 and libc++ on macOS arm64.

## Subject & scope
- **Subject:** `std::optional`, `std::variant`/`std::visit`,
  `std::expected`; `std::ranges::` algorithms, `std::views::filter`,
  `transform`, `take`, `reverse`; borrowed and owning views; iterator
  categories; structured bindings; if-init; `std::format`,
  `std::format_to_n`, `std::print`; `std::string_view`, `std::span`,
  `std::byte`, `std::from_chars`; CTAD; `[[nodiscard]]`.
- **Grain:** choose the right vocabulary type, predict when a view does work
  and when it dangles, and spot the hidden copy, allocation or overread.
- **Runnable samples:** `lab/*.cpp`; each file's third line is its build
  command (run from the tutorial root; programs go to your temp directory).

## Time cost (honest)
- **First pass:** about 149 minutes over 5 lessons (38 + 29 + 26 + 34 + 22; lesson 05 is the LPA lens on errors),
  counted as reading at about 200 words a minute plus about 1 minute per
  pretest, 1.5 per completion card, 2 per question and 4 per synthesis card,
  plus about 1 minute per lab program you build. 44 cards: 7 pretests,
  7 completions (M4-LPA3 is a fill-in completion), 15 questions, 9 gotchas, 4 judgement cards, 2 synthesis.
- **Optional practice bank:** lesson 06 (`lesson-06-practice-bank.md`), about 10 minutes,
  2 self-graded cards on measurement, research and C++ techniques and on how
  results expire. It is optional and not counted in the first pass above; its
  runnable programs are in `lab/practice/`.
- **Reviews:** the driver schedules misses for the next day and uninterrupted hits about
  2, 9, 30, 51 and 72 days after the first attempt; each sitting is capped by the driver.
- **Interruptible:** yes; progress is saved after every card.

## Learning objectives
1. Given a C++98 function that returns a sentinel or uses an out-parameter,
   the learner can rewrite it to return `std::optional`, use it without
   undefined behaviour, and state what `value()` and `*` do when it is
   empty.
2. Given a tagged union, the learner can replace it with `std::variant` and
   a `std::visit` callable for every alternative, and compare it with virtual
   dispatch without claiming either
   always wins.
3. The learner can return failure with `std::expected` and say what it does
   and does not enforce.
4. The learner can write a `filter | transform` pipeline, predict when its
   lambdas run, and avoid the const-iteration and dangling traps of views.
5. The learner can say what a type needs to be a `std::ranges::range` and
   which iterator category an algorithm or view requires.
6. The learner can pass text and buffers with `string_view`/`span`, find the
   dangling and missing-terminator bugs, parse with `from_chars`, and predict
   CTAD results for braced vector initialisation.
7. The learner can use structured bindings and if-init without unintended
   copies, and format text with explicit output bounds and checked runtime costs.
8. LPA lens on errors (lesson 05): say which percentile a rare throw reaches
   at a given error rate, explain its cost, and write the hot-path check that
   returns `std::unexpected` with no exception machinery.

## Tiers supported
```
tiers: [T0, T1, T2, T3]
detect:
  - model + tools -> T3   (Socratic, adaptive, published transfer items only)
  - model, no tools -> T2 (grades free text vs Key bullets)
  - python3 drive.py -> T1 (regex AcceptIf / self-report, no model)
  - else -> T0            (open lessons/*.md and self-quiz)
```
Run T1 with:
`python3 ../../drive.py lessons --progress progress.json --today $(date +%Y-%m-%d)`

Card types, topics, objectives, source links, grading rules and time budgets are in [cards.json](cards.json).
