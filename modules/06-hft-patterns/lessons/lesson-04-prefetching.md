# Lesson 04 — Software Prefetch: A Hint That Is Easy to Get Wrong

Time: about 40 minutes (12 minutes reading, a 5-minute measurement, 9 cards).

**The hook.** The lab compares independent random gathers from a 256 MiB
table with dependent accesses through a separate 128 MiB table. Independent
requests can overlap; dependent requests expose more of the waiting time.
Compare time per operation, and do not mistake throughput for single-load
latency.

**You will be able to:** write a `__builtin_prefetch` call and say what each
argument does, predict when prefetch cannot help, and measure whether it does.

**You need from earlier:** module 05 (cache lines) and lesson 03 (measure,
don't guess).

## Pretest — guess first

### Card M6-P4 · Pretest
Q: You probe a hash table and cache misses dominate. You already know the
   bucket address you will read 5 iterations from now. Guess: (a) what kind of
   instruction could start fetching that line now without waiting for it?
   (b) Give one reason it might make the loop slower.
<details><summary>Answer</summary>
Key:
  - (a) A prefetch instruction. GCC and Clang expose it as
    `__builtin_prefetch(addr, rw, locality)`: it asks for the line and the
    program continues without waiting.
  - (b) Any of: the CPU was already overlapping these reads on its own; the
    prefetch adds instructions and address arithmetic; a line fetched too early
    can be evicted before use or push out data you still need. The only way to
    know is to measure.
</details>

---

### CONCEPT C9  `__builtin_prefetch`: what it is and what each argument does

**The hash table that does not trust its own prefetches.** Open the hash
table code in Velox and you find prefetch hints with conditions attached.
When grouping rows, it knows the accumulators sit "behind the key, often on
the next cache line", so it asks for the end of the row early, to overlap
that fetch with other probe work.

The same comment goes on: "Skip rows narrower than 64 bytes, where the
microbenchmarks show no gain." Another loop goes further: it times its own
first 16 iterations with a clock, then picks how far ahead to prefetch,
clamped between 4 and 32 iterations.

Intel's optimization manual explains the caution. A hint must arrive early
enough to overlap work, and when data is already in the first-level cache,
prefetching "can be useless and could even slow down the performance". So
treat a prefetch as a bet: ask early, then verify that it helped your loop.

**The compiler bridge.** ISO C++ has no standard prefetch operation. GCC and
Clang provide `__builtin_prefetch`, including in older language modes, so
one source expression can target several CPUs.

```cpp
__builtin_prefetch(addr);              // rw = 0 (read), locality = 3 (defaults)
__builtin_prefetch(&bucket[h], 0, 0);  // read, no temporal locality
```

| argument | meaning (GCC manual) | values |
|---|---|---|
| `addr` | address of the memory to prefetch | an expression safe to evaluate |
| `rw` | preparing to read or write | `0` read (default), `1` write; GCC also documents `2` |
| `locality` | how much reuse to hint ("temporal locality") | `0` none ... `3` high (default) |

Both `rw` and `locality` must be compile-time constants. Clang 21 rejected
`rw = 2` ("argument value 2 is outside the valid range [0, 1]") and rejected
a variable `locality` ("must be a constant integer").

**What it compiles to** (Apple clang 21, `-O2`; x86-64 via `--target`):

| call | arm64 | x86-64 |
|---|---|---|
| `(a, 0, 0)` | `prfm pldl1strm` | `prefetchnta` |
| `(a)` = `(a, 0, 3)` | `prfm pldl1keep` | `prefetcht0` |
| `(a, 0, 1)` | `prfm pldl3keep` | `prefetcht2` |
| `(a, 1, 0)` | `prfm pstl1strm` | `prefetchnta` |

**The address is still evaluated.** The GCC manual: a prefetch "does not
generate faults if addr is invalid, but the address expression itself must
be valid". So `__builtin_prefetch(p->next)` can use an initialized pointer
that is not safe to dereference, but reading `p->next` requires a valid `p`.
Likewise `&v[i + 16]` with
`std::vector::operator[]` past the end is already undefined behaviour before
any prefetch happens. Bound the lookahead:

```cpp
if (i + 16 < n) __builtin_prefetch(&v[i + 16], 0, 0);
```

**Rule:** a prefetch requests a line while your code continues. The `rw` and
locality arguments are compile-time constants. The address may be computed
at runtime, and evaluating it must be valid.

### Card M6-C4 · Fix it
Q: A hash probe has computed `&buckets[h]`, will read it soon, and will never
   touch it again. Complete the call that requests it as a read with no
   temporal locality:
   `______________(&buckets[h], __, __);`
   Answer format: type the whole call on one line, for example
   `name(&buckets[h], X, Y)`.
<details><summary>Answer</summary>
Key:
  - `__builtin_prefetch(&buckets[h], 0, 0)`: `rw = 0` means read and
    `locality = 0` requests no temporal locality, suitable for a one-shot
    access. It is a hint, not a cache-residency guarantee.
</details>

### Card M6-Q7 · Recall
Q: (a) What does the third argument (`locality`) of `__builtin_prefetch`
   control, and what is its default? (b) When would you pass `0`?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Temporal locality, 0 to 3: how strongly the line should be kept in
    cache after the access. The default is 3 (keep in all levels possible).
  - (b) For data you touch once and will not revisit, so that fetching it is
    less likely to push out data you do reuse.
</details>

---

### CONCEPT C10  When prefetch helps, and measuring it

**Why distance matters.** A hint issued one instruction before the load has
almost no work to overlap; one issued too early may fetch a line that is gone
by the time you use it.

**Prefetch distance.** To hide the whole miss, the fetch must finish before
demand and the line must remain available. A later fetch can still hide part
of the latency. Measure whether that saving exceeds the extra instructions
and memory traffic. If one iteration takes `w` ns and a memory
read takes `L` ns, a first guess for the distance is about `L / w`
iterations ahead. It is a starting point for measurement, not an answer.

**Measure it yourself** (about 3 seconds, about 450 MiB of memory):

```sh
clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/prefetch/gather.cpp \
  -o "${TMPDIR:-/tmp}/gather" && "${TMPDIR:-/tmp}/gather"
```

The program prints time per operation for several gather distances, a
sequential scan and a dependent pointer chase. Compare distances on the same
run; the absolute numbers depend on the machine and background work.

**Reading it, step by step.** The random *gather* reads `table[idx[i]]`:
every address is known in advance from `idx`, so several reads can overlap.
The *pointer chase* reads `at = next[at]`: the next address is the value
just loaded, so requests are more dependent. A gather's time per lookup is
amortized throughput, not the latency of one isolated load. Prefetch might
save part of a miss and still lose overall when its instructions and memory
traffic cost more. Measure each distance and keep only a repeatable win.

**Linked lists.** While processing node `p`, you can prefetch `p->next` once
`p` has been read. You cannot prefetch five nodes ahead: finding node +5 means
loading nodes +1 to +4 first. `__builtin_prefetch(p->next->next, 0, 0)` first reads the next node's
link, so it cannot hide the miss needed to obtain that link. Prefetch
`p->next` while doing useful work on the current node. If the structure can be an array, future addresses become
computable.

**The trap.** "Prefetch makes memory-bound code faster." The hint can also
waste instructions or displace useful data.

**Rule:** prefetch only an address you can compute cheaply, far enough
ahead, and keep it only if a measurement on the target machine shows a win.

| first change | cost and promise | use when | wrong-choice smell |
|---|---|---|---|
| change data layout | changes storage and code; can make future addresses computable | a pointer chain can become an array | migration cost exceeds measured gain |
| software prefetch | extra instructions and cache traffic; only a hint | address is known early but misses still stall | sequential loop becomes slower |
| leave it alone | no new complexity; current measured behaviour | hardware overlap is already enough | tail remains above budget |

Try layout before hints when the algorithm permits it; a prefetch with an
unknown future address is the smell that layout is the real problem.

### Card M6-J4 · Apply
Q: The `index` values come from incoming messages, so the loop skips bad
   ones. An AI assistant added a guarded prefetch two lookups ahead and says
   "the guard keeps it in bounds". Is the address use right here? What would
   you check first, and what would change your verdict?
   Answer format: three short sentences.
   ```cpp
   #include <cstddef>
   #include <vector>
   int sum_valid(const std::vector<int>& table, const std::vector<std::size_t>& index) {
       int sum = 0;
       for (std::size_t i = 0; i < index.size(); ++i) {
           if (i + 2 < index.size())
               __builtin_prefetch(&table[index[i + 2]], 0, 0);
           if (index[i] < table.size()) sum += table[index[i]];   // skip bad ones
       }
       return sum;
   }
   int main() { return sum_valid({10, 20, 30, 40}, {3, 0, 2, 1}) == 100 ? 0 : 1; }
   ```
<details><summary>Answer</summary>
Key:
  - Not for message input: the guard keeps `i + 2` inside `index`, but the prefetch forms `&table[index[i + 2]]` without the check the read has, so a bad index is undefined behaviour before any prefetch happens.
  - First check where the `index` values come from and whether each one is checked before every use, including the prefetch.
  - If the indices were validated before this loop, the code would be right;
    for message input, add `index[i + 2] < table.size()` to the prefetch
    guard, then keep the hint only if it measures faster.
</details>

### Card M6-Q8 · Apply
Q: You walk a singly linked price level: for each `Level* p` you read `p->qty`,
   then `p = p->next`. (a) Which address can you prefetch while working on
   `p`? (b) In one sentence, why can you not prefetch 5 nodes ahead as you
   could with an array of indices?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `p->next`, for example `if (p->next) __builtin_prefetch(p->next, 0, 0);`,
    after `p` itself has been read; it overlaps the next node's fetch with the
    current node's work. Whether that helps depends on how much work each node
    takes, so measure.
  - (b) Node +5's address is stored in node +4, which is only known after
    loading nodes +1 to +4: a dependency chain. With an array the future
    address is computed directly from the index.
</details>

### Card M6-G8 · Spot the trap
Q: A developer adds `__builtin_prefetch(&v[i + 16], 0, 0);` inside a loop that
   sums a `std::vector<int> v` from start to end. (a) In one sentence, why is
   it unlikely to speed up this loop? (b) What is wrong with the expression
   near the end of the vector, and how do you write it safely?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) The scan is sequential, which the hardware already handles well:
    added prefetches can consume instructions and cache bandwidth even when
    they hide part of a miss; measure before keeping them.
  - (b) For the last 16 iterations `v[i + 16]` indexes past the end, which is
    undefined behaviour before the prefetch even runs. Guard it:
    `if (i + 16 < v.size()) __builtin_prefetch(&v[i + 16], 0, 0);`.
</details>

### Card M6-G9 · Spot the trap
Q: (a) To hide pointer-chasing misses, a developer writes this fragment:
   ```cpp
   struct Node { Node* next; int value; };
   // node and node->next are valid; node is already available.
   __builtin_prefetch(node->next->next, 0, 0);
   ```
   In one sentence, why does this not hide the miss? (b) In one sentence, what goes wrong if the prefetch
   distance is far too short?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Computing this request first reads the next node's link, so it cannot
    hide the miss needed to obtain that link. Prefetch `node->next` while
    doing useful work on the current node.
  - (b) A short distance leaves residual waiting. It may still reduce the
    wait, but the overhead can outweigh the saving.
</details>

---

## Integrate — tie the module together

### Card M6-INT1 · Choose and explain
Q: Design the data access for the loop that scans one price level's resting
   orders to match an incoming order. Say what you would do about
   (a) data layout, (b) software prefetch, (c) the rare "trading halted"
   branch, and in which order you would try them. A short paragraph.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Layout first: store the level's orders contiguously (an array or ring
    of fixed-size records) instead of a linked list, so addresses are
    computable and the scan is sequential.
  - (b) Prefetch only if a part must stay irregular (for example a hash lookup
    into another table), only with a bounded, computable address, and keep it
    only if a measurement shows a win.
  - (c) Leave the common path falling through: use profile data, or
    `[[unlikely]]` on the halt check if you know it is rare, and check the
    assembly.
  - Order: layout, then measure; then prefetch or hints one at a time, each
    re-measured, because they can cancel out.
</details>

### Card M6-INT2 · Choose and explain
Q: Capstone: an SPSC ring (lesson 01) feeds a hot handler (lessons 02 to 04).
   (a) Which atomic operations in the ring use release and acquire, and why is
   that enough? (b) Name two hot-path rules you would enforce in the handler.
   (c) How would you measure its latency without fooling yourself?
   One or two sentences each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) The producer writes the slot, then stores `head_` with release; the
    consumer loads `head_` with acquire before reading the slot (and `tail_`
    uses the same pair in the other direction). Each counter has one writer,
    and release/acquire on it orders the slot write before the slot read.
  - (b) Any two of: no heap allocation; no system calls or I/O (log through a
    ring to a cold thread); no blocking locks; no throwing; pre-touch pools to move initial demand faults to startup;
    verify residency separately.
  - (c) Drive it open-loop at a realistic rate, report tail percentiles rather
    than an average, use hardware arrival timestamps where available, and fence
    any tick-counter reads.
</details>

---

## Closing

Remember: a prefetch is a hint whose address must already be valid; you
cannot prefetch what you do not know yet; measure before you keep a hint.
Cards you missed come back tomorrow. Next: lesson 05, the tail-budget
capstone (then module 07, the tools around the code).
