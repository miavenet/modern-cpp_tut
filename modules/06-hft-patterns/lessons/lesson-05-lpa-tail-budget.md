# Lesson 05 — Capstone: Who Owns the Tail?

**Time: about 30 minutes** (reading about 8, the lab about 12 including three 2-second runs, cards about 10). The lab is resumable: your work file stays where you left it.

**Hook:** one program, one planted 300 µs stall, two ways to drive it. Driven like most test clients, it met its latency budget in all 7 runs. Driven like a real market feed, it broke the budget in all 7, with about 75 times as many slow messages. Same code, same stall. Only the *measurement* changed.

**You will be able to:** explain coordinated omission in plain words, drive a test open-loop from intended send times, read per-stage histograms against a stated budget, and name the stage that *owns* the tail, which is not always the stage where it *shows*.

**Needs from earlier:** lesson 01 (the SPSC ring), lesson 03 (concept C6, coordinated omission; concept C11, the course histogram and its sample rules). Module 03 lesson 05 (spinning hand-offs) helps.

## Pretest — commit your guess FIRST

### Card M6-LPA4 · Pretest
Q: A feed thread hands 1,000,000 messages, one every 2 µs, through an SPSC ring to a strategy thread. Ten times during the run the strategy stops for a 300 µs snapshot. Client A sends each message only after the previous one is finished (then waits 2 µs). Client B sends on its own clock, every 2 µs no matter what. Each counts messages slower than 50 µs, measured from when the message *should* have been sent. Guess both counts.
<details><summary>Answer</summary>
Key:

  - A: about 17 (median of 7 runs, range 10 to 59). B: about 1,284 (range 1,198 to 3,301).
  - While the strategy is stuck, A sends nothing, so each stall delays just one message. B keeps sending: about 150 messages arrive during each 300 µs stall, and the first 125 or so of them wait longer than 50 µs. Ten stalls × 125 ≈ 1,250, which is what B counted.
</details>

---

## Driving the test like the market does

### CONCEPT C12  LPA capstone: open-loop measurement against a budget

**Coordinated omission, in plain words.** A test that waits for each answer before asking the next question *slows down exactly when the system does*. During a stall it stops sending, so the messages that a real feed would have sent during the stall never exist, and they never get a slow time. The test and the stall "coordinate" to leave the bad part out. The fix: decide *in advance* when each message is due (its *intended* send time), send on that schedule whatever happens, and measure every latency from the intended time. A message that could not even be sent on time then counts its lateness too.

The *Latency Prioritized Approach* (LPA) judges by the far tail against a stated budget. The three questions, for this capstone:

- **Default does / LPA does:** the default test is closed-loop and reports one end-to-end number. LPA drives open-loop from intended send times, states a budget up front, and records one histogram *per stage* so a broken budget points at a stage.
- **Why (mechanism):** a stall of length *S* in a stream arriving every *T* delays about *S* / *T* messages, not one. Here 300 µs / 2 µs = 150 messages queue behind each snapshot: the stall is *owned* by the strategy, but it *shows* as queue wait for everyone behind it.
- **Gives up:** an open-loop test is harder to write (a schedule, timestamps in every message, a ring big enough that the feed never blocks), its numbers look worse (they are true), and the busy-polling threads cost two whole cores.

**The lab.** `lab/budget/solution/tail_budget.cpp`. Budget: end-to-end **p99.9 ≤ 20 µs and p99.99 ≤ 50 µs**. Three runs, each in a fresh child process, 1,000,000 messages each:

- *closed loop:* the feed waits for each reply, then one interval (fixed code, for contrast);
- *open loop:* the feed follows `intended_send_ns()`, which is your code;
- *open, sliced:* the same schedule, but the snapshot is cut into 1 µs slices, one per message.

Stages: **send lateness** = sent − intended (feed thread); **queue wait** = dequeued − sent (ring, and the strategy being busy); **service** = done − dequeued (the strategy's own work); **end-to-end** = done − intended.

**What we measured.** Course Mac: Apple M1 Max, macOS 27, Apple clang 21, `-O2`, two busy-polling threads, no core pinning, shared machine (1-minute load average 8.6 to 10.3, 5-minute 11.9 to 12.6). Median [lowest–highest] of 7 runs.

| Run | end-to-end p99.9 | end-to-end p99.99 | over 50 µs | budget met |
|---|---|---|---|---|
| closed loop | 4,479 ns [3,583–6,015] | 20,479 ns [13,311–26,111] | 17 [10–59] | 7 of 7 |
| open loop | 98,303 ns [92,159–208,895] | 286,719 ns [286,719–376,831] | 1,284 [1,198–3,301] | 0 of 7 |
| open, sliced | 14,847 ns [7,679–33,791] | 34,815 ns [16,383–442,367] | 52 [0–853] | 4 of 7 |

Per stage, open loop (medians of 7 runs, ns):

| Stage | p50 | p99 | p99.9 | p99.99 | max |
|---|---|---|---|---|---|
| send lateness | 42 | 83 | 7,551 | 22,527 | 102,875 |
| queue wait | 125 | 639 | 92,159 | 286,719 | 299,083 |
| service | 0 | 42 | 125 | 167 | 300,084 |

**Read it.** Service's own p99.99 is 167 ns: only 10 of a million messages took a snapshot (0.001%), too few for even p99.99 to see. Its *max* (300 µs, in every run) is the owner's fingerprint. Queue wait carries the echo: 1,500 messages queued behind those 10. Send lateness stays under the budget at p99.9: the feed was on time.

**An honest surprise from building it.** In a first version, all three runs shared one process. Whichever open-loop run came *second* was far worse (the sliced run's p99.9 was 655 µs that way (median of 7 runs), against 11 µs when it ran first (median of 5 runs with the order swapped)). The likely cause is the scheduler deprioritising threads that have been busy for seconds; we did not confirm it. The fix was a fresh process per run. Run order is part of a benchmark.

**Rule:** decide each message's due time in advance, measure from it, histogram every stage, and look for the owner at the stage whose *max* explains the others. Fine print: after you fix the owner, the next one appears; on a shared Mac it is often the OS.

### Card M6-LPA5 · Build and check
Q: Do the lab: write `intended_send_ns` in `modules/06-hft-patterns/lab/work/tail_budget.cpp` so the feed keeps its own clock. Run it, then submit that path.

   Step 0 (from the tutorial root; the `cp` OVERWRITES an earlier work copy):
   ```sh
   mkdir -p modules/06-hft-patterns/lab/work && cp modules/06-hft-patterns/lab/budget/starter/tail_budget.cpp modules/06-hft-patterns/lab/work/
   clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure -I modules/06-hft-patterns/lab/spsc/solution modules/06-hft-patterns/lab/work/tail_budget.cpp -o "${TMPDIR:-/tmp}/tail_budget" && "${TMPDIR:-/tmp}/tail_budget"
   ```
   The starter runs only the closed loop (and prints `RESULT: TODO`). **Predict** first: will your open-loop run meet the budget? Then write the function, rebuild, and read the three runs. Check with
   `printf '%s\n' modules/06-hft-patterns/lab/work/tail_budget.cpp | modules/06-hft-patterns/checks/tail_budget.check.sh`.
   It checks the schedule with fixed numbers and no clock, never a time.
Hint: message *i* is due *i* intervals after the start. What the previous reply did is not your schedule's business.

<details><summary>Answer</summary>
Key:

  - `return start_ns + i * kIntervalNs;` (and drop `prev_done_ns`, for example `(void)prev_done_ns;`).
  - Success: three runs print; the open loop breaks the budget by about 5x at p99.9; the program prints `RESULT: PASS`. Your exact numbers will differ.
  - The check calls your function with fixed starts, indexes and "previous reply" times. Waiting for the reply (`std::max(..., prev_done_ns)`), pacing from `lat::now_ns()`, and starting one interval late all fail.
</details>

### Card M6-LPA6 · Apply
Q: In the open-loop run above, which stage *owns* the p99.9 budget breach: `feed` (send lateness), `queue` (queue wait) or `service`?
   Answer format: one word: feed, queue or service.

<details><summary>Answer</summary>
Key:

  - `service`. Its p99.9 is only 125 ns, but its max is 300 µs in every run: the snapshot. That one stall makes about 150 messages queue behind it, which is why queue wait has the big p99.9 (92 µs). Queue wait shows the tail; service owns it. Send lateness (7.6 µs at p99.9) was within the budget, so the feed was on time.
  - The evidence that it is the owner: slicing the snapshot (service max 300 µs → about 17 µs) dropped end-to-end p99.9 from 98 µs to about 15 µs.
</details>

### Card M6-LPA7 · Recall
Q: In plain words, what is coordinated omission, and what two things did this lab do to avoid it?
   Answer format: one line; separate the definition and the two fixes with semicolons.

<details><summary>Answer</summary>
Key:

  - A test that waits for each reply stops sending during a stall, so the requests that would have arrived during it never get measured: the test hides the stall.
  - The feed sends on a schedule fixed in advance (`start + i × interval`), whatever the strategy is doing.
  - Every latency is measured from the message's *intended* send time, so a late send counts its lateness too.
</details>

### Card M6-LPA8 · Synthesize
Q: After slicing, the budget held in 4 of 7 runs. In the 3 failed runs, send lateness reached p99.99 values of 0.12 to 0.38 ms while service stayed near 1 µs. (a) Which stage owns the tail now, and what is the likely mechanism? (b) Name one LPA fix and what it gives up, and say what this Mac lets you do about it.
   Answer format: two or three sentences.

<details><summary>Answer</summary>
Key:

  - (a) The feed (send lateness): the feed thread itself was not running when messages were due, most likely the OS preempting it on a shared machine (load average above the core count). Open-loop accounting counts every message it was late for.
  - (b) On Linux: give the feed and strategy dedicated, isolated cores (`isolcpus`, pinning, interrupts moved away). That gives up two whole cores, and the machine can do less else. macOS has no pinning or isolation: the honest options are a quieter machine (fewer competing jobs, normal QoS), or stating the budget as unmet on this platform.
</details>

---

## Close

**Remember:** a closed-loop test slows down with the system and hides its stalls; fix the schedule in advance and measure from it. A stall shows downstream as queue wait, but the owner is the stage whose max explains it. Fix one owner and the next appears. **Tomorrow:** missed cards come back. This is the last lesson of module 06.
