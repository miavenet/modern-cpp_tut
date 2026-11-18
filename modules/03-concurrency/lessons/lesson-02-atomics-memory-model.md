# Lesson 02 — Atomics and the Memory Model: When One Thread Can See Another's Writes

**Hook:** two threads each add 1 to a counter 100,000 times. The result
can look plausible while the program has a data race. **You will be able to:** spot a data race, publish data from
one thread to another with release/acquire, choose the cheapest correct
memory order, and write a compare-exchange loop.

**Time:** about 32
minutes. **You need:** lesson 01 (threads and join).

## Pretest — commit your guess first

### Card M3-P2 · Pretest
Q: Thread A writes a plain `int x` while thread B reads `x`, with no mutex
   and no atomics. What does the C++ standard say happens? Guess in one
   line.
<details><summary>Answer</summary>
Key:

  - It is a *data race*, and a data race makes the behaviour of the whole
    program undefined.
  - It is not "B sees either the old or the new value". The compiler may
    optimise on the assumption that no race exists, so any result is
    possible.
</details>

---

## Data races and happens-before

### CONCEPT C3  Data race: the definition that decides everything

**"Threads cannot be implemented as a library."** That was the blunt title of
a paper by Hans Boehm. Before C++11, a thread library could start two workers,
but the language did not tell the optimizer what their shared writes meant.

His claim was stranger than it sounds: "a C++ compiler can introduce data races
where there were none in the source." No library can fix that; it is
"fundamentally a language specification and compiler issue".

The folk remedy, marking every shared field `volatile`, fails too. Boehm quotes
Dave Butenhof: "The use of "volatile" is not sufficient to ensure proper memory
visibility or synchronization between threads."

The C++11 memory model gives the compiler and the programmer one rulebook, and a
clean-looking run cannot replace it.

**Words first.**
- A *memory location* is the storage of a scalar object other than a bit-field,
  or a maximal sequence of adjacent nonzero-width bit-fields.
- Conflicts include read/write or write/write accesses to the same location,
  and overlapping accesses involving the start or end of an object's lifetime.
- An *atomic* operation is one the language treats as indivisible; you get
  them through `std::atomic<T>` (C++11, header `<atomic>`).
- A *data race* is two conflicting accesses from different threads, at
  least one not atomic, where neither *happens before* the other (next
  concept). The standard says any data race is undefined behaviour.

**The C++98 situation.** C++98 had no threads, so it had no rule at all for
this. C++11 added the memory model: the rules above.

**See it yourself: `lab/race.cpp`.**

```cpp
// excerpt of lab/race.cpp (the includes are there)
int counter = 0;                       // plain int, shared

void work() {
    for (int i = 0; i < 100000; ++i) ++counter;   // read, add, write
}

int main() {
    std::thread a(work), b(work);
    a.join();
    b.join();
    std::cout << "counter = " << counter << " (200000 expected)\n";
}
```

`++counter` reads and writes. If both threads read 5 and then write 6,
one increment is lost; the data race makes actual program behaviour undefined. The lab command builds it with
`-fsanitize=thread`. *ThreadSanitizer* (TSan) watches memory accesses while
the program runs and reports races it sees:

```
WARNING: ThreadSanitizer: data race (pid=...)
  Write of size 4 at ... by thread T2:
    #0 work() race.cpp:...
```

The observed counter is not a defined result to rely on. The race makes the
whole program undefined, even when it prints 200000.

**The fix: `lab/fixed.cpp`.** Make the counter `std::atomic<int>` and use
`counter.fetch_add(1, std::memory_order_relaxed)`. After joining both workers, it prints 200000; the atomic counter has no race.

**The trap.** "An `int` store is one instruction on my CPU, so it is
atomic anyway." The rule is about C++, not your CPU: the compiler may keep
the value in a register or merge the loop.

**Rule:** if one thread writes a shared variable while another thread
accesses it, the variable must be atomic, or one access must happen before
the other.


### CONCEPT C4  Happens-before: publishing data with release and acquire

**Why this exists.** An atomic flag can be read safely while the payload
behind it is still unsafe. The language needs an edge from the payload
write to the payload read, not merely an indivisible flag.

**Words first.**
- *Sequenced before*: inside one thread, an earlier statement is sequenced
  before a later one.
- *Synchronizes with*: a store with `memory_order_release` to an atomic
  synchronizes with an acquire operation that reads its value or a value
  from its release sequence. Reading the initial `false` in this example
  establishes no publication edge.
- *Happens before*: a chain made of those two links. If a write happens
  before a read, the read sees that write (or a later one), and the two are
  not a data race.

**See it yourself: `lab/publish.cpp`** (prints `100 101`; TSan silent).

```cpp
// excerpt of lab/publish.cpp (there each part runs in its own std::thread)
struct Quote { int bid; int ask; };
Quote quote;                       // plain payload
std::atomic<bool> ready{false};    // the flag that publishes it

// producer thread
quote = {100, 101};                              // (1)
ready.store(true, std::memory_order_release);    // (2)

// consumer thread
while (!ready.load(std::memory_order_acquire)) {} // (3)
std::cout << quote.bid << ' ' << quote.ask;       // (4)
```

```
producer                          consumer
(1) quote = {100,101}
     | sequenced before
(2) store(true, release) ==syncs with==> (3) load(acquire) reads true
                                              | sequenced before
                                         (4) read quote
So (1) happens before (4): (4) sees {100, 101}.
```

**Now break it.** Change both orders to `memory_order_relaxed`. Relaxed
creates no synchronizes-with link, so (1) and (4) are a data race on
`quote`: undefined behaviour. TSan reports races it detects; a clean run
is evidence, not proof.

**Why "it works on x86".** x86 hardware keeps stores in order with other
stores, so the broken version often behaves. AArch64 (Apple silicon,
Graviton) may make two independent stores visible to another core in either
order. The compiler may also reorder relaxed and plain accesses on any CPU.

**Rule:** publish this payload with a release store and an acquire load that
reads it. Fine print: an acquire can also read a release sequence. In this
flag-only hand-off, relaxed accesses alone do not publish the payload.


---

### Card M3-C2 · Fix it
Q: Fill in the weakest memory orders that still make this message hand-off
   correct (`msg_buffer` is a plain `std::string`):
   ```cpp
// Fragment: fill the blanks in this exercise.
   // Producer
   msg_buffer = "hello";
   flag.store(true, std::memory_order____);

   // Consumer
   while (!flag.load(std::memory_order____)) {}
   print(msg_buffer);
   ```
   Answer format: one line, the two order names in order, separated by a
   comma.
<details><summary>Answer</summary>
Key:

  - `release, acquire`
  - The release store synchronizes with the acquire load that reads `true`,
    so the write to `msg_buffer` happens before the read. (`seq_cst` on both
    would also be correct, but it is stronger than needed; `relaxed` is a
    data race on `msg_buffer`.)
</details>

### Card M3-J2 · Apply
Q: A market-data poller checks once per tick whether the producer's one
   snapshot is ready, and must never block: if it is not ready, it skips the
   tick. A reviewer objects that the AI-written acquire load is not in a loop,
   so it "might read the payload before the producer wrote it". Is the code
   correct here? Answer format: three short sentences: decision, first check, rejection evidence.
   ```cpp
   #include <atomic>
   #include <cstdio>
   #include <thread>
   int payload = 0;
   std::atomic<bool> ready{false};
   int main() {
       std::thread producer([] { payload = 42; ready.store(true, std::memory_order_release); });
       std::thread poller([] {
           if (ready.load(std::memory_order_acquire)) std::printf("got %d\n", payload);
           else std::puts("not yet");
       });
       producer.join(); poller.join();
   }
   ```
<details><summary>Answer</summary>
Key:

  - The payload publication is data-race-free, but the poller does not meet
    the no-blocking requirement: synchronous output can wait.
  - First inspect every operation on the tick path, including diagnostics.
  - Reject `printf`/`puts` on that path; record the result locally and report
    it elsewhere. Also reject moving the payload read before the successful acquire.
</details>

---

### Card M3-Q9 · Recall
Q: A producer writes a plain payload, then stores `true` to an atomic flag.
   Which memory orders on the store and the consumer's successful load
   publish the payload? Answer format: one sentence.
<details><summary>Answer</summary>
Key:

  - Use release on the store and acquire on the load that reads the release
    (or its release sequence), so the payload write happens before its read.
</details>

### Card M3-G3 · Spot the trap
Q: A plain (non-atomic) payload struct is published through an
   `std::atomic<bool>` flag, using `memory_order_relaxed` for both the store
   and the load. Tests pass on an x86 workstation; on an AWS Graviton (ARM)
   server the consumer sometimes reads stale payload fields.
   (a) What is wrong in C++ terms? (b) Why did x86 hide it?
   Answer format: two short lines, (a) and (b).
<details><summary>Answer</summary>
Key:

  - (a) Relaxed creates no synchronizes-with link, so the payload write does
    not happen before the payload read: a data race on the payload, which is
    undefined behaviour on every CPU. Fix: release store, acquire load.
  - (b) x86 keeps stores in order with other stores, so the flag rarely
    becomes visible before the payload; ARM may reorder independent stores.
    Passing on x86 never proves the code correct.
</details>

---

## Choosing a memory order

### CONCEPT C5  The memory orders, and what they cost in instructions

**Why choose an order?** Default `seq_cst` gives a simple shared ordering
story. Weakening it is useful only after you identify what the atomic must
publish. Begin with the meaning of the counter or flag, then inspect code
generation and measurements.

**The menu** (all C++11, header `<atomic>`):

| Order | Use on | Gives you |
|---|---|---|
| `relaxed` | any op | the operation is atomic; no ordering of other memory |
| `acquire` | loads and read-modify-write operations | can read a release or its release sequence |
| `release` | stores and read-modify-write operations | can publish preceding writes to an acquire that reads it |
| `acq_rel` | read-modify-write (e.g. `fetch_add`) | both of the above |
| `seq_cst` (the default) | any op | acquire/release, plus one total order of all `seq_cst` operations that every thread agrees on |

| Choice | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| `relaxed` | Atomic value only; no payload ordering | Independent counter | A reader treats a flag as proof a payload is ready |
| release/acquire | Publishes preceding writes when the load reads the store | One-way hand-off | The acquire reads the wrong flag or an old value |
| `seq_cst` | Adds one order across `seq_cst` operations | You need the simpler global reasoning | It was weakened solely from an unmeasured speed guess |

Advice: draw the required happens-before arrow before weakening an order;
an unexplained payload read is the smell that the order is wrong.

**When relaxed is enough.** When no other data depends on the atomic, only
its own value matters. `lab/fixed.cpp` is the example: a counter read after
the threads are joined (the join itself orders the final read).

Code generation for Apple clang 21 at `-O2`, using
`-target x86_64-apple-macos -S` for the x86-64 column:

| Operation | arm64 | x86-64 |
|---|---|---|
| store relaxed | `str` | `mov` |
| store release | `stlr` | `mov` |
| store seq_cst | `stlr` | `xchg` |
| load relaxed | `ldr` | `mov` |
| load acquire | `ldapr` | `mov` |
| load seq_cst | `ldar` | `mov` |

So on x86-64 the visible difference is the `seq_cst` store: `xchg` with
memory is a locked instruction that also acts as a full barrier, usually
dearer than a plain `mov`. On arm64 the release and `seq_cst` stores are
the same instruction. Reproduce the table yourself with the `-S` flag, and
measure before you claim a speed-up.

**The trap.** "`seq_cst` is always slow, so avoid it." It is the safe
default; weaken an order only when you can say which happens-before edge
you still need.

**Rule:** start with the default, weaken to release/acquire for
publication, and use relaxed only for values nothing else depends on.


---

### Card M3-Q4 · Apply
Q: Several threads `fetch_add` an atomic counter of processed orders. A
   dashboard reads it now and then; no other data is published through it.
   Which is the weakest correct memory order for the `fetch_add`?
   Answer format: one word.
<details><summary>Answer</summary>
Key:

  - `relaxed` (`std::memory_order_relaxed`).
  - The update is still atomic, so no increment is lost; nothing else needs
    to be ordered by it, so acquire/release would add nothing.
</details>

### Card M3-G4 · Spot the trap
Q: Release and sequentially consistent stores compile to the same instruction
   on one target. Does that prove that replacing all sequentially consistent
   operations with release/acquire preserves the program? What would you check
   first? Answer format: two short sentences.
<details><summary>Answer</summary>
Key:

  - No. First prove the ordering the algorithm needs, including constraints
    involving multiple atomics. Identical instructions for one isolated
    operation do not establish equivalent whole-program behavior. Then inspect
    and measure the complete workload.
</details>

---

## Compare-and-exchange

### CONCEPT C6  compare_exchange: weak, strong, and the retry loop

**Why this exists.** A load followed by a store cannot safely update a
value that another thread may change between those steps. Compare-exchange
tests the expected old value and performs the replacement as one atomic
operation; failure tells you to reconsider the new value.

**What it does.** In C++20, compare-exchange compares value representations,
not `operator==`. If the representations match, it stores `desired`; otherwise
it writes the observed representation into `expected`. Weak compare-exchange
may additionally fail spuriously; strong compare-exchange does not.

**Spurious failure.** The standard says a *weak* compare-exchange "may fail
spuriously": return `false` even though the values were equal. It does not
name a cause. Its note says this permits machines built on
load-linked/store-conditional, where the store half can fail.

**Worked example: a maximum update using CAS; lock-free where `atomic<int>`
is lock-free:**

```cpp
// C++11 fragment
void update_max(std::atomic<int>& hi, int v) {
    int cur = hi.load(std::memory_order_relaxed);
    while (cur < v && !hi.compare_exchange_weak(cur, v)) {
        // failed: cur now holds the latest value; the loop re-checks it
    }
}
```

Walk it: `hi` is 5; thread A calls `update_max(hi, 7)`, thread B calls
`update_max(hi, 9)`. Both read `cur = 5`. B's exchange succeeds: `hi = 9`.
A's fails, and `cur` becomes 9. A re-checks `9 < 7`: false, so it stops.
`hi` ends at 9.

**What clang emits.** For a plain ARMv8.0 target, the weak form is one
`ldaxr`/`stlxr` attempt and the strong form adds a retry branch. On this
Mac's default CPU both become a single `casal` instruction.

**The trap.** "Weak fails because of cache-line bouncing." The standard
names no cause; just treat failure as possible at any time.

**Rule:** use weak inside a retry loop; use strong when one attempt must
give a meaningful answer.


---


### Card M3-Q3 · Recall
Q: Without looking back, write `update_max(std::atomic<int>& hi, int v)`
   using weak compare-exchange. Explain why a failed exchange can make another
   attempt unnecessary. Answer format: code, then one sentence.
<details><summary>Answer</summary>
Key:

  - `int cur = hi.load(std::memory_order_relaxed); while (cur < v &&
    !hi.compare_exchange_weak(cur, v)) {}`
  - Failure refreshes `cur`; another thread may already have installed a
    value at least as large as `v`. Weak can also fail spuriously.
</details>

---

## Closing

Remember: a data race is undefined behaviour, whatever your CPU does.
Release/acquire publishes when the acquire reads the release or its release sequence.
Use weak compare-exchange in a loop. Misses come back tomorrow. Next:
lesson 03, mutexes, spinlocks, cache lines, latches and barriers.
