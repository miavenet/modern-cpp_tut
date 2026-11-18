# Lesson 01 — The SPSC Ring: Handing Data Between Two Threads Without a Lock

Time: about 40 minutes (17 minutes reading, a 10-minute lab, 7 cards).

**Module 06 adds** the patterns that sit on a trading system's critical path:
a queue with lock-free bookkeeping between threads (this lesson), hot-path rules (02),
honest latency measurement (03) and software prefetch (04).
**It assumes** module 03 (atomics, `memory_order`) and module 05 (cache lines).
Words such as "data race" are re-defined here in one line, so you can start cold.

**The hook.** Your market-data thread decodes a quote. Your strategy thread must
see it, and the hand-over's queue bookkeeping must never block. The usual C++98 tool, a
`std::queue` guarded by a mutex, can put a thread to sleep. This lesson builds
the alternative: a ring of slots and two atomic counters, about 30 lines.

**You will be able to:** explain why one `release` store and one `acquire` load
make the hand-over safe, check it with ThreadSanitizer, and say why the design
breaks with two producers.

**How to run the samples.** Every command runs from the tutorial root.
Build output goes to your temporary folder (`${TMPDIR:-/tmp}`), never into the
course folder.

## Pretest — guess first, wrong guesses are fine

### Card M6-P1 · Pretest
Q: A ring buffer has two atomic counters: `head_` (written only by the producer
   thread) and `tail_` (written only by the consumer thread). They sit next to
   each other in memory, in the same 64-byte block. Guess in one sentence: why
   could that make both threads slower, even though they never write the same
   counter?
<details><summary>Answer</summary>
Key:
  - Caches move memory in fixed blocks called cache lines. If both counters sit
    in one line, ownership can bounce between cores when they alternately
    write their different counters. This is false sharing.
  - The fix is to give each counter its own cache line (for example with
    `alignas`). How much time false sharing costs depends on the machine;
    measure it rather than assume a figure.
</details>

---

### CONCEPT C1  The SPSC ring: two counters, one release, one acquire

**Three minutes and 44 seconds to count to 500 million.** The LMAX team,
building a financial exchange, measured what a lock really costs. One thread
incrementing a 64-bit counter 500 million times took 300 ms.

Add a lock that nobody else wants: 10,000 ms. Let two threads fight over it:
224,000 ms. Even a lock-free compare-and-swap took 30,000 ms once two threads
contended for the same counter.

So their Disruptor ring preallocates every slot, uses a power-of-two size so
the slot index is a bit mask, and gives each counter a single writer. With
one producer, the paper says, "no locks or CAS operations are required".

The ring below is a much smaller SPSC teaching design, not the Disruptor, but
its two counters make the same single-writer bet.

The queue bookkeeping uses lock-free atomics and has no retry loop. The
complete operation is bounded and nonblocking only if copying `T` is too.
`noexcept` alone does not establish that.

**Words first.** SPSC means *single producer, single consumer*: exactly one
thread pushes and exactly one other thread pops. A *data race* is two threads
touching the same plain (non-atomic) memory, at least one of them writing,
with nothing ordering the two accesses; it is undefined behaviour.
*Happens-before* is the C++ rule that orders such accesses. A `release` store
to an atomic, read by an `acquire` load that sees the stored value, makes
everything the storing thread did before the store visible to the loading
thread after the load.

**The C++98 way and why it hurts.**

```cpp
// C++98 (pthread): every push and pop takes the lock.
pthread_mutex_lock(&m);  q.push(quote);  pthread_mutex_unlock(&m);
```

If the other thread holds the lock, this thread waits, and a contended lock
can put a thread to sleep until the operating system wakes it. On a hot path
you want queue bookkeeping that never waits.

**The modern way (C++11 atomics, C++20 `std::has_single_bit`).** A fixed array
of `N` slots, plus two ever-growing counters. `head_` counts pushes and
`tail_` counts pops. The ring holds `head_ - tail_` items. The slot for count
`c` is `c % N`; with `N` a power of two that is `c & (N - 1)`.

```
N = 4      slot:     0      1      2      3
           holds:  [ q4 ] [ q5 ] [ -- ] [ q3 ]
tail_ = 3  -> next pop reads slot 3 & 3 = 3 (q3)
head_ = 6  -> next push writes slot 6 & 3 = 2     items = 6 - 3 = 3
```

This fragment is the two member functions of the class in
`modules/06-hft-patterns/lab/spsc/solution/spsc_ring.hpp`, which declares
`head_`, `tail_`, `slots_`, `kMask` and `N` and pads each counter onto its own
128-byte cache line (the size `sysctl hw.cachelinesize` reports on the Apple
silicon Mac used to test this course; check your own machine):

```cpp
bool try_push(const T& v) noexcept {                     // producer thread only
    const std::uint64_t h = head_.load(std::memory_order_relaxed);  // my own counter
    const std::uint64_t t = tail_.load(std::memory_order_acquire);
    if (h - t == N) return false;                        // full
    slots_[h & kMask] = v;                               // 1. write the data
    head_.store(h + 1, std::memory_order_release);       // 2. then publish it
    return true;
}
bool try_pop(T& out) noexcept {                          // consumer thread only
    const std::uint64_t t = tail_.load(std::memory_order_relaxed);  // my own counter
    const std::uint64_t h = head_.load(std::memory_order_acquire);  // 3. see the publish
    if (h == t) return false;                            // empty
    out = slots_[t & kMask];                             // 4. now the data is visible
    tail_.store(t + 1, std::memory_order_release);       // 5. hand the slot back
    return true;
}
```

**Walk one item through.** From the picture, the producer writes q6 into
`slots_[2]` (step 1), then stores `head_ = 7` with release (step 2). Later the
consumer, having popped q3 to q5, loads `head_` with acquire and sees 7
(step 3). Release-then-acquire on the same counter means
step 1 happens-before step 4: the consumer reads the finished quote, never a
half-written one. The second pair (release on `tail_` in step 5, acquire on
`tail_` in `try_push`) protects the other direction: the producer cannot
overwrite a slot the consumer is still reading.

Each thread reads its *own* counter `relaxed`: nobody else writes it, so there
is nothing to synchronise with.

**Why a power of two?** It lets unsigned `c % N` use a bit mask. This is
what Apple clang 21 at `-O2` produced on arm64 when this course was tested:

```
c % 1024   ->  and  x0, x0, #0x3ff          (one instruction)
c % 1000   ->  a multiply-high and shifts    (constant divisor, no divide)
c % n      ->  udiv + msub                   (n known only at run time)
```

Other ring designs accept any capacity; this one chooses powers of two so
the index is a mask. A `static_assert(std::has_single_bit(N))` enforces it.

**See it yourself: the lab (about 10 minutes).** The starter file
`modules/06-hft-patterns/lab/spsc/starter/spsc_ring.hpp` has two lines marked
`TODO(M6-C1)`, both using `memory_order_relaxed`. Build it with
ThreadSanitizer (TSan, a compiler option that reports data races it observes
while the program runs):

```sh
clang++ -std=c++20 -O1 -g -fsanitize=thread -I modules/06-hft-patterns/lab/spsc/starter \
  modules/06-hft-patterns/lab/spsc/test_spsc.cpp -o "${TMPDIR:-/tmp}/spsc" && "${TMPDIR:-/tmp}/spsc"
```

First run on the test machine: all five value tests print `ok` and
`RESULT: PASS`, yet TSan prints `WARNING: ThreadSanitizer: data race` on the
line `out = slots_[t & kMask];`, ends with `ThreadSanitizer: reported 1
warnings`, and the program exits with a non-zero status. Fix the two
`TODO(M6-C1)` lines in the starter, rerun the same command, and success is
`RESULT: PASS` with no TSan warning. The answer is in `.../lab/spsc/solution/`.
To have the course check your edited file (prints `HIT` or `MISS`):
`printf '%s\n' modules/06-hft-patterns/lab/spsc/starter | modules/06-hft-patterns/checks/spsc_lab.check.sh`

**The trap.** "The tests pass, so it is correct." The starter passes every
value test and still has a data race, which is undefined behaviour: it may
work today and fail with another compiler, CPU or load.

**Rule:** publish with a `release` store after writing the slot; read the
counter with an `acquire` load before reading the slot.

| hand-off | cost and guarantee | right when | wrong-choice smell |
|---|---|---|---|
| SPSC ring | fixed capacity; one writer per counter; `try_push` can fail | one producer, one consumer | a second caller pushes |
| mutex queue | locking and possible wait; several callers can share it | producers vary or simplicity matters | a measured lock wait dominates the deadline |
| MPMC ring | fixed capacity; per-slot sequence numbers and retries | several producers or consumers need one queue | contention or stalled publishers grow the tail |

Use the simplest queue whose caller count and full-queue policy match reality;
the smell of a wrong pick is changing the caller count without changing the queue.

### Card M6-C1 · Fix it
Q: In the lab file `modules/06-hft-patterns/lab/spsc/starter/spsc_ring.hpp`,
   the two lines marked `TODO(M6-C1)` are:
   ```cpp
// Fragment: fill the blanks in this exercise.
   head_.store(h + 1, std::memory_order____);           // in try_push
   const std::uint64_t h = head_.load(std::memory_order____);  // in try_pop
   ```
   Give the weakest memory order that is still correct for each blank.
   Answer format: one line, two words separated by a comma, in file order
   (store first, then load). Example shape: `word1, word2`.
<details><summary>Answer</summary>
Key:
  - `release, acquire`: the store in `try_push` is `release` (it publishes
    the slot written just before it) and the load in `try_pop` is `acquire`
    (it must see that slot before reading it).
</details>

### Card M6-Q1 · Recall
Q: Why does a power-of-two capacity simplify ring indexing? Does a constant
   non-power-of-two divisor necessarily require a hardware divide, and how
   would you check?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - For unsigned counters, a power-of-two capacity permits a bit mask.
    Other constant divisors can also be optimized without division. Inspect
    the target's optimized assembly; do not infer the cost from `%` alone.
</details>

### Card M6-Q2 · Apply
Q: A colleague wrote every atomic operation in the ring as
   `memory_order_seq_cst` "to be safe". (a) Is the ring still correct?
   (b) On which of the two CPUs in the table below does that choice change the
   instruction emitted for the `head_` store? Answer (a) yes or no, (b) the CPU name.

   | operation on `head_` | arm64 (this Mac) | x86-64 |
   |---|---|---|
   | release store | `stlr` | `mov` |
   | seq_cst store | `stlr` | `xchg` |
   | acquire load  | `ldapr` | `mov` |
   | seq_cst load  | `ldar` | `mov` |
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Yes. `seq_cst` is at least as strong as release and acquire, so the
    ring stays correct; it is just stronger than this design needs.
  - (b) x86-64: the seq_cst store becomes `xchg` instead of a plain `mov`.
    On arm64 the store is `stlr` either way (the loads differ: `ldar` vs
    `ldapr`). Whether any of this costs measurable time depends on the CPU and
    workload; measure before and after.
</details>

---

### CONCEPT C2  Many producers: why SPSC breaks, and a bounded MPMC ring

**Why another queue exists.** The moment a second producer arrives, the
single-writer promise disappears; making only the counters atomic does not
assign a unique slot to either writer.

**Why SPSC breaks with two producers.** Each producer loads `head_` relaxed,
writes the slot, then stores `head_ + 1`. Two producers can both load
`head_ == 6`, both write `slots_[2]`, and both store 7. One quote silently
overwrites the other, and the two plain writes to `slots_[2]` are a data
race. Nothing in the type stops you: SPSC is a contract about *who calls it*.
Two fixes: give each producer its own SPSC ring (the consumer polls them
all), or use a queue designed for several producers, like the one below.

**A bounded MPMC ring** (multi-producer, multi-consumer) in the style of
Dmitry Vyukov's queue gives each slot its own sequence number:

```cpp
// Fragment of lab/mpmc/mpmc_ring.hpp (T, N, cells_, enqueue_pos_, pos, v
// are declared there).
struct Cell { std::atomic<std::uint64_t> seq; T data; };
// try_push, inside a retry loop:
Cell& c = cells_[pos & (N - 1)];
std::uint64_t seq = c.seq.load(std::memory_order_acquire);   // 1. is this slot free for lap `pos`?
if (seq == pos &&
    enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {  // 2. claim it
    c.data = v;                                               // 3. write
    c.seq.store(pos + 1, std::memory_order_release);          // 4. publish to consumers
}
```

A producer first checks the slot's sequence, then claims the position with
compare-exchange (an atomic "replace if still equal" operation; on failure it
reloads and retries), writes, and publishes with release. A consumer later
stores `pos + N` into `seq`, which marks the slot free for the next lap. The
full tested version is `modules/06-hft-patterns/lab/mpmc/mpmc_ring.hpp`;
`test_mpmc.cpp` next to it moves 400,000 values through 4 producers and
4 consumers in the lab. A clean TSan run does not prove race freedom.

**Two honest limits.** First, this algorithm is not lock-free in the formal
sense: a thread paused between step 2 and step 4 leaves its slot unpublished,
and consumers of that slot must wait. The libcds documentation for its Vyukov
queue likewise calls it "not lock-free in the official meaning". Second, it is a bounded array: `try_push` fails
when full.

**ABA, in one paragraph.** A compare-exchange checks only "is the value still
A?". If another thread changed it A to B and back to A, the check passes
although the world changed. This classically bites pointer-based structures
when a freed node's address is reused. Here the compared values are 64-bit
position counters that only grow, and slots are never freed, so a repeat of an
old value would need the counter to wrap around.

**Rule:** SPSC is correct only with exactly one pushing thread and one popping
thread; for more, use a structure designed for it and know its progress limits.

### Card M6-J1 · Apply
Q: One feed thread pushes quotes into the lab's `SpscRing`. When the primary
   feed goes stale, an AI assistant's failover code hands pushing to a backup
   thread and says "still only one producer at a time". Is it right here?
   What would you check first, and what would change your verdict?
   Answer format: three short sentences.
   ```cpp
   // Fragment, inside a function: ring is the lab's SpscRing<int, 1024>;
   // one consumer thread pops elsewhere. Needs <atomic> and <thread>.
   std::atomic<bool> stop{false};
   std::thread primary([&] { int q = 0; while (!stop.load()) ring.try_push(++q); });
   // ... later, the primary feed goes stale, so fail over:
   stop.store(true);
   std::thread backup([&] { for (int q = 0; q < 1000; ++q) ring.try_push(-q); });
   primary.join();
   backup.join();
   ```
<details><summary>Answer</summary>
Key:
  - Not as written: `primary` can still be inside a `try_push` when `backup` starts, so for a moment two threads push and can claim the same slot.
  - First check what guarantees the old producer's last push finishes before the new producer's first one; here nothing does, because `primary.join()` comes after `backup` starts.
  - Moving `primary.join()` above the line that starts `backup` makes it right, because the join orders every primary push before the backup runs; a clean TSan run cannot prove this narrow window is safe.
</details>

### Card M6-G1 · Spot the trap
Q: Four strategy threads push log events into one `SpscRing`; one logger thread
   pops. Staging looks fine; production logs show lost and garbled events. In
   two sentences: what goes wrong inside `try_push`, and what do you use instead?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - Two producers can read the same `head_` value, write the same slot, and
    store the same new `head_`: one event overwrites the other, and the
    unsynchronised writes to the slot are a data race (undefined behaviour).
  - Use one SPSC ring per producer (the logger polls all four), or a queue
    built for several producers such as a bounded MPMC ring.
</details>

### Card M6-G2 · Spot the trap
Q: (a) In one sentence, what is the ABA problem for a compare-exchange?
   (b) In one sentence, why does the bounded MPMC ring in this lesson not
   suffer from it in practice?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) A compare-exchange only checks that the value still equals what was
    read; if another thread changed it away and back (A to B to A), the
    exchange succeeds although the state it guarded has changed.
  - (b) Its compared values are 64-bit position counters that only increase and
    its slots are never freed or reused at a new address, so seeing an old value
    again would require the counter to wrap around.
</details>

---

## Closing

Remember: write the slot, then `release`-store the counter; `acquire`-load
the counter, then read the slot. Passing tests do not prove there is no race;
TSan reports the races it sees in a run. SPSC means exactly one producer.
Cards you missed come back tomorrow. Next: lesson 02, the rules for the hot
path that this ring feeds.
