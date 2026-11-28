# Lesson 06 — Practice Bank, part 3 of 12 (optional): Which Span a Latency Number Covers

**Time: about 21 minutes (estimate)** for 3 cards (6 + 5 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: datasheet replies, one-way RPC latency, and service time against waiting time.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB8 · Apply
Q: A field-bus module's datasheet says it "replies to a 16-byte command in 1.5 us". The bus runs at 1 Mb/s over 300 m of cable. Before anyone argues about it, compute: how many bits arrive in 1.5 us at that rate; how long the whole command takes to arrive; and the least time a signal needs to travel the cable out and back. Which pairs of events can the 1.5 us lie between, and which can it not? Check your numbers:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/bus_window.cpp -o "${TMPDIR:-/tmp}/bus_window" && "${TMPDIR:-/tmp}/bus_window"
   ```
   Answer format: the three numbers, then which spans are impossible and which one is possible.
<details><summary>Answer</summary>
Key:

  - 1 Mb/s x 1.5 us = 1.5 bits. The command is 128 bits and takes 128 us to arrive.
  - So 1.5 us cannot run from "first command bit leaves the master" (or arrives at the module) to "first reply bit leaves the module" for a module that reads the whole command.
  - Light in vacuum, 299 792 458 m/s, needs 1.00 us for 300 m, so at least 2.00 us out and back. Real cable is slower. So 1.5 us also cannot span "master sends" to "master sees the reply".
  - What it can span: some interval inside the module, for example from the last command bit it needs to the first reply bit leaving. That may be true and still useless for a bus budget.
  - Habit: for any headline latency, do the bits-per-window and distance-per-window arithmetic first, then ask for the start and end events.
  - Holds for / re-check: raw bit rate only; line coding and framing overhead only lower the usable bit count. Redo the arithmetic when rate, message size or cable length changes: at 10 Mb/s the same window holds 15 bits and the command takes 12.8 us. The speed of light is an exact defined constant (SI defining constants, BIPM) and does not expire. The cable is assumed to carry the signal at no more than c; real cables are slower, which only strengthens the point.
</details>

### Card M6-PB9 · Spot the trap
Q: An RPC library's page gives "one-way latency: 90 us", defined as half of a round trip timed from just before the client encodes the request to just after it decodes the reply, between two hosts. A teammate builds a budget for the span "client starts encoding the request to server finishes decoding it": 90 (one-way) + 12 (encode) + 12 (decode) + 40 (network crossing) = 154 us. Assume both directions cost the same, with 12 us to encode, 12 us to decode and 40 us for one crossing, and that there are no other costs before the server finishes decoding: all remaining round-trip time is server handling after that event and before it encodes the reply. What is wrong, and what is left of the 90 once you account for the parts already in it?
   Answer format: the error in one line, the span's figure, then what the leftover 26 us is.
<details><summary>Answer</summary>
Key:

  - By that definition the round trip contains two encodes, two decodes and two network crossings. Half of it already contains one of each.
  - So adding encode, decode and the crossing on top of the 90 counts them twice: 154 - 64 = 90, the whole published figure is surplus for this span.
  - For the span the teammate named, the given parts alone give 12 + 40 + 12 = 64 us. The 90 is not a figure for that span: under the stated assumption the round trip is 12+40+12 (request) + server handling + 12+40+12 (reply) = 180, so server handling is 52 us and half of it, 26 us, sits in the 90 but after the named span ends. Without that assumption the 26 us could equally be queueing before decode, which would sit inside the span (the span would then be 90 and the 52 us of handling would be zero): the facts alone do not place the residual, so say which assumption you make.
  - If the budget is meant for the client's full request-to-reply time, the answer is the whole round trip, 2 x 90 = 180 us, not 154. Either way, the total depends on which span the budget is for, so write the span down first.
  - You can only do this split after reading the definition. Under another definition (say, timed on the wire) the answer changes.
  - Holds for / re-check: arithmetic on invented figures (the no-other-costs-before-decode assumption in the question is what fixes the 64 us and 52 us answers); the part sizes are given, not measured. The method applies to any published "half round trip" or "one-way" figure. Re-read the definition whenever the vendor changes the test set-up.
</details>

### Card M6-PB10 · Apply
Q: `spans.cpp` lets a burst of 64 messages arrive, then one worker handles them in order. It records two spans per message: service time (handler start to handler end) and response time (message arrival to handler end). Predict: (a) for how many of the 12 800 messages is response >= service? (b) For the last message of each burst, how does its response time compare with the sum of all 64 service times? (c) Which median is larger, and what happens to the gap with a burst of 1? Run:
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/spans.cpp -o "${TMPDIR:-/tmp}/spans" && "${TMPDIR:-/tmp}/spans" 64 && "${TMPDIR:-/tmp}/spans" 1
   ```
   Answer format: (a), (b), (c), then the question to ask when someone quotes a latency.
<details><summary>Answer</summary>
Key:

  - (a) All 12 800. The response span contains the service span, so it can never be shorter.
  - (b) In 200 of 200 bursts the last message's response time is at least the sum of all service times in the burst. It waited for every message ahead of it.
  - (c) With a burst of 64 the median response time is far larger than the median service time (same direction in 3 runs on each of three compilers). With a burst of 1 the two medians are about equal.
  - Same code, same run, two honest numbers that differ by a large factor. A latency figure without its start and end events cannot be compared with another one.
  - When someone quotes the small one, ask: where did the clock start?
  - Holds for / re-check: (a) and (b) follow from the order of the clock reads and hold on any machine with a monotonic clock. (c) is a direction only; the size of the gap depends on burst size and handler cost, and was observed under an unknown desktop load (one machine, varies with load). Checked with Apple clang 21 (macOS arm64), and clang 18.1.3 and GCC 13.3 (Linux aarch64).
</details>

---

## Close

Next: part 4, `lesson-06-practice-bank-04-percentiles-bins.md` (about 26 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
