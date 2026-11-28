# Lesson 06 — Practice Bank, part 7 of 12 (optional): Branches and the Predictor

**Time: about 22 minutes (estimate)** for 2 cards (10 + 12 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: benchmarking separately predicted checks, and a reference point for a misprediction rate.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB17 · Apply
Q: You want to benchmark an order check with five separately predicted branches. `checks.cpp` writes the same five range checks three ways: `ok_early` (five `if (...) return false;` lines), `ok_andand` (one `return a && b && c && d && e;`) and `ok_bitand` (the same with `&`). Predict how many conditional-branch instructions each function has at `-O2`. Then count them (the script is for arm64 listings; it counts `b.<cond>`, `cbz`, `cbnz`, `tbz`, `tbnz`, and separately compare-and-select instructions such as `ccmp` and `cset`). If you have a second compiler, repeat with it, and once at `-O0`:
   ```sh
   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/checks.cpp -o "${TMPDIR:-/tmp}/checks.s" && sh modules/06-hft-patterns/lab/practice/count_branches.sh "${TMPDIR:-/tmp}/checks.s"
   ```
   Answer format: counts per function per compiler, then what "number of branches" should mean in a benchmark.
<details><summary>Answer</summary>
Key:

  - The source does not tell you. Five `if` lines gave 1 conditional branch with Apple clang 21 and 4 with both GCC 13.3 and clang 18.1.3, same flags, same CPU family.
  - `ok_early` and `ok_andand` compile to the same count as each other on every compiler tested at `-O2` (1 and 1, or 4 and 4). Writing `&&` instead of early returns changed nothing.
  - `ok_bitand` has 0 conditional branches on all three compilers. GCC 13.3 used `ccmp`/`cset`; both clangs used a NEON vector compare (`cmhi` + `umaxv`) plus one `cset`, and no `ccmp`. Either way there is no branch for the predictor to see.
  - At `-O0` the count follows the source (5 for `ok_early` on all three), so an unoptimized build can mislead you about the optimized one.
  - Consequence for any branch benchmark: "number of branches" has two meanings, sites (one branch instruction at one address) and instances (one execution of it). A loop that runs one `if` a million times has one site and a million instances. State which one you vary, and count the sites in the listing, because the compiler may merge or remove them.
  - Holds for / re-check: counts are for Apple clang 21.0.0 (arm64) and for GCC 13.3.0 and clang 18.1.3 (aarch64 Linux). They will differ on x86 (different instructions; the script's patterns are arm64 only) and can change with any compiler release or flag. That is the point: re-count after every toolchain or flag change.
</details>

### Card M6-PB18 · Apply
Q: A profile shows one branch mispredicted 25% of the time. A teammate says the predictor is "failing badly" on it. Before agreeing, you need a reference line. `predictor_model.cpp` simulates three simple predictors in software and counts their mistakes exactly: (a) always guess the more common outcome, (b) one 2-bit saturating counter (a tiny state machine that moves toward "taken" or "not taken" by one step per outcome), (c) a table of such counters indexed by the last 8 outcomes. It feeds them independent random outcomes taken 50%, 40%, 25%, 15% and 2% of the time, and then a repeating pattern taken-not-not-not that is also 25% taken. Predict the miss rate of (a), (b) and (c) for random 25% and for the repeating 25%. Then run:
   ```sh
   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/practice/predictor_model.cpp -o "${TMPDIR:-/tmp}/predictor_model" && "${TMPDIR:-/tmp}/predictor_model"
   ```
   Answer format: three rates for random 25%, three for the repeating pattern, then the rule.
<details><summary>Answer</summary>
Key:

  - For outcomes independent of all information the predictor has (and with no other branch exposing the next outcome), the minimum expected miss rate is min(p, 1-p), the best constant-prediction baseline; finite samples fluctuate. Always guessing the common side misses min(p, 1-p) of the time. Model output: 0.5000, 0.4000, 0.2497, 0.1499, 0.0199 for the five cases (equal to the realized taken rate, or one minus it).
  - Neither learning predictor beats that line on random outcomes. The 2-bit counter is worse: 0.2997 at p=0.25, 0.4609 at p=0.40, 0.1710 at p=0.15. The 8-outcome history table is no better (0.2997, 0.4616, 0.1708), because past random outcomes say nothing about the next one. Accept about 0.30 for both learning predictors on random 25% input.
  - So 25% misses on a branch that is taken 25% of the time, with random outcomes the predictor cannot otherwise see, is already about the best available. The predictor is not failing; the data is unpredictable. For a real profile treat min(p, 1-p) as the baseline to compare against, not as an unconditional bound: another branch may reveal the next outcome.
  - The repeating pattern has the same 25% taken rate but a different answer: majority guess 0.2500, 2-bit counter 0.2500, history table 0.0000 (3 warm-up misses). Structure in the order of outcomes, not the taken rate, is what a history predictor uses.
  - Rule: judge a measured miss rate against the min(p, 1-p) baseline for that branch, not against zero. If the rate is far below that line, the outcomes have a pattern the predictor learned. Then ask whether your benchmark input has that same pattern in production.
  - Holds for / re-check: a software model with a fixed seed, so the mistake counts are deterministic and were identical with Apple clang 21 and GCC 13.3 (Linux); displayed rates are rounded to four decimals. It says nothing about how large or clever a real CPU's predictor is. The min(p, 1-p) line is arithmetic and does not expire. Whether a real core reaches it, or learns a given pattern, depends on the CPU and must be measured there with a mispredict counter.
</details>

---

## Close

Next: part 8, `lesson-06-practice-bank-08-branch-data.md` (about 21 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
