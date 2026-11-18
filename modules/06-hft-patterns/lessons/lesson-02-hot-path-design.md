# Lesson 02 — Hot-Path Design: Do the Slow Work Before the Market Opens

Time: about 30 minutes (13 minutes reading, a 2-minute run, 7 cards).

**The hook.** Your strategy reacts to a quote, builds an order and sends it.
Every line on that path either takes a bounded number of steps or *might*
call into the allocator, the kernel or the unwinder, which take as long as
they take. This lesson is about moving the "might" lines off the path.

**You will be able to:** name the operations a hot path avoids and the
mechanism behind each, count page faults yourself, and move logging and
message encoding off the hot thread.

**You need from earlier:** the SPSC ring from lesson 01.

## Pretest — guess first

### Card M6-P2 · Pretest
Q: The "hot path" is the code between a market-data packet arriving and the
   order leaving. Guess: name three kinds of operation a low-latency hot path
   avoids, and in a few words why.
<details><summary>Answer</summary>
Key:
  - Heap allocation (`new`, `malloc`): allocator code of unpredictable length,
    which may ask the operating system for more memory.
  - System calls and I/O (`write`, `send`, `syslog`): they enter the kernel.
  - Blocking locks: a contended `std::mutex` can make the thread wait.
  - Throwing exceptions: a throw creates an exception object and runs the
    stack unwinder; the runtime may allocate.
  - The shared reason: their duration is not bounded by your code. Do them
    before trading starts, or on another thread.
</details>

---

### CONCEPT C3  The hot-path contract, and the page fault you did not write

A fast average can hide an expensive first request. Move allocation and
initial page touches to startup, then measure the complete hot path under
realistic traffic.

**C++98 habits that hide unbounded work.** None of these lines looks slow:

```cpp
std::string symbol = msg.symbol();        // may allocate
orders.push_back(o);                      // may reallocate and copy
log << "sent " << id << std::endl;        // formats, flushes, enters the kernel
if (qty <= 0) throw BadQty();             // may allocate; unwinds
```

**The contract, with the mechanism behind each rule:**

| avoid on the hot path | why (mechanism) | instead |
|---|---|---|
| heap allocation | allocator code; may ask the OS for memory | preallocate: pools, `std::array` (C++11) |
| system calls, I/O | each one enters the kernel | hand the work to another thread |
| blocking locks | a contended mutex can make you wait | the SPSC ring from lesson 01 |
| `throw` | creates an exception object, runs the unwinder; may allocate | return codes, `std::expected` (C++23) |
| virtual calls in the tightest loop | indirect call; cannot be inlined unless the compiler proves the type | templates or `std::variant`; measure |

Evidence for two rows, from Apple clang 21 at `-O2` on this course's test
machine: a function containing `throw` calls `__cxa_allocate_exception`
then `__cxa_throw`; a call to a virtual `on_tick` loads the function address
from the object (`ldr x8, [x0]`, ...) and jumps through a register (`br x2`).

**The page fault you did not write.** You preallocate a 64 MiB pool with
`new char[...]`. The operating system usually hands out the physical memory
only when each page is first touched. That first touch is a *page fault*: the
CPU stops your code and the kernel maps the page. If the first touch happens
during trading, the fault happens during trading.

**See it yourself** (counts faults with `getrusage`, which macOS and Linux both provide):

```sh
clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/hotpath/first_touch.cpp \
  -o "${TMPDIR:-/tmp}/first_touch" && "${TMPDIR:-/tmp}/first_touch"
```

Output on the test Mac (16 KiB pages, so 64 MiB is 4096 pages):

```
faults during first touch:  4096
faults during second touch: 0
```

**The fix:** touch (write) every page of every pool at startup. This moves
initial demand faults; it does not prevent later eviction or copy-on-write.
Where supported, memory locking can request residency. Check its return
value and resource limits before relying on it.

**The trap.** "No `new` on the hot path, so no memory surprises." The
allocation was early; the faults still arrive at first use.

**Rule:** keep hot-path work bounded and move initial pool faults to startup.
Fine print: touching pages does not keep them resident or prevent later faults.

### Card M6-C2 · Fix it
Q: Fill the blanks to use fixed-size storage and the SPSC ring. Assume
   `SentEvent` has bounded, nonblocking copy assignment.
   ```cpp
// Fragment: fill the blanks in this exercise.
   struct NewOrder { std::_____<char, 8> symbol{}; std::int64_t px; };  // blank 1: fixed size, no heap
   _____<SentEvent, 4096> log_ring;   // blank 2: the SPSC class from lesson 01
   ```
   Answer format: one line, two names separated by a comma, blank 1 first.
<details><summary>Answer</summary>
Key:
  - `array, SpscRing`: `std::array<char, 8>` (C++11) stores its 8 characters
    inside the struct, so there is no heap allocation; `SpscRing` is lesson
    01's ring. The queue bookkeeping uses lock-free atomics and has no
    retry loop. The complete operation is bounded and nonblocking only if
    copying `T` is too. `noexcept` alone does not establish that.
</details>

### Card M6-G3 · Spot the trap
Q: You allocate all pools at startup and never call `new` again, yet the first
   minutes of trading show stalls, and a profiler blames page faults.
   (a) In one sentence, why do faults still happen? (b) Which startup step
   moves initial first-write faults out of trading, and what does it not guarantee?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) The operating system usually maps physical memory only when a page is
    first touched, so the first write to each pool page faults, and that
    first write happened during trading.
  - (b) Write every pool page at startup. This handles initial demand faults
    but does not keep pages resident. Where supported, check memory-locking
    success and limits, and account separately for later allocations and
    copy-on-write.
</details>

---

### CONCEPT C4  Pre-encoded messages and hot/cold logging

**Why split the work?** Formatting a log line when the quote arrives puts the
logger's work on the quote's deadline. A fixed event record lets the hot
thread hand over facts and leaves text formatting to the cold thread.

**Pre-encoded messages.** Most bytes of an outgoing order are the same every
time: message type, symbol, account. Build the whole byte array once at
startup; on the hot path copy only the fields that change into their fixed
offsets. Here is a made-up 24-byte layout (fictional, for illustration),
excerpted from `modules/06-hft-patterns/lab/hotpath/pre_encoded.cpp`
(the full file includes `<string_view>` and `<stdexcept>`; validation runs
only at startup):

```cpp
struct OrderTemplate {
    std::array<unsigned char, 24> bytes{};
    explicit OrderTemplate(std::string_view symbol) {      // cold: once at startup
        if (symbol.size() > 8)
            throw std::invalid_argument("symbol exceeds 8 bytes");
        bytes[0] = 'N';
        bytes[1] = 'O';
        std::memset(&bytes[2], ' ', 8);
        if (!symbol.empty())
            std::memcpy(&bytes[2], symbol.data(), symbol.size());
    }
    void set(std::uint32_t qty, std::int64_t price_ticks) {    // hot: two fixed-size copies
        std::memcpy(&bytes[10], &qty, sizeof qty);
        std::memcpy(&bytes[14], &price_ticks, sizeof price_ticks);
    }
};
```

`OrderTemplate msg("MSFT"); msg.set(100, 4105000);` printed:

```
4e 4f 4d 53 46 54 20 20 20 20 64 00 00 00 28 a3 3e 00 00 00 00 00 00 00
```

`4e 4f` is "NO", then "MSFT" and four spaces, then 100 (`64 00 00 00`) and
4105000 (`28 a3 3e 00 ...`), both little-endian as stored by this Mac. No
loop, no formatting, no allocation at send time.

**Hot/cold logging.** Formatting text and writing it are the slow part of a
log line. The hot thread pushes a small fixed-size struct into an SPSC ring;
a separate "cold" thread pops it, formats and writes
(`modules/06-hft-patterns/lab/hotpath/hot_cold_log.cpp`, fragment):

```cpp
struct SentEvent { std::uint64_t order_id; std::int64_t price_ticks; };  // 16 bytes
void hot_path_send(std::uint64_t id, std::int64_t px) {
    // ... send the order ...
    if (!log_ring.try_push({id, px})) { /* full: count a dropped line, never block */ }
}
// cold thread: while (log_ring.try_pop(e)) std::printf("sent id=%llu ...", ...);
```

The full file prints `sent id=1 px=4105001` and two more lines.

**The trap.** Making the ring "never lose a line" by spinning until
`try_push` succeeds. Now a slow disk on the cold thread stalls trading.
Decide what a full ring means (drop and count for optional telemetry) up front.

**Rule:** prepare bytes and expensive formatting off the hot path; the hot
path copies a few fixed-size fields and hands off.

| full log ring policy | hot-thread cost | guarantee | right when / warning smell |
|---|---|---|---|
| drop and count | one failed attempt | trading continues; that line is lost | optional telemetry; loss count must be visible |
| spin until space | unbounded waiting | line enters if the consumer makes space | only if waiting is allowed; deadline stalls are the smell |
| block on a mutex | may sleep | queue access is serialized | cold paths; a lock wait in the hot trace is the smell |

Choose the full-ring policy before trading starts; if lost logs are unacceptable,
redesign capacity or the data path instead of hiding a wait in `try_push`.

### Card M6-J2 · Apply
Q: Your compliance rule says every sent order must appear in the audit
   trail. An AI assistant used this lesson's recommended drop-and-count policy
   for the audit events on the order path. Is it right here? What would you
   check first, and what would change your verdict?
   Answer format: three short sentences.
   ```cpp
   // Build with -I modules/06-hft-patterns/lab/spsc/solution
   #include <cstdint>
   #include "spsc_ring.hpp"
   struct AuditEvent { std::uint64_t order_id; std::int64_t px; };
   SpscRing<AuditEvent, 4096> audit_ring;
   std::uint64_t audit_dropped = 0;
   void after_send(std::uint64_t id, std::int64_t px) {
       if (!audit_ring.try_push({id, px})) ++audit_dropped;   // never block
   }
   int main() { after_send(1, 4105000); return audit_dropped == 0 ? 0 : 1; }
   ```
<details><summary>Answer</summary>
Key:
  - Wrong for this record: it keeps the order thread fast, but a full ring silently loses an audit event that the rule says must exist.
  - First check what the requirement says must happen when a record cannot be queued, and how often the ring fills at peak load.
  - For mandatory audit records, reserve capacity before sending the order. If reservation fails, reject that order before transmission. Commit and retain the event under the required durability and recovery policy; stopping later orders cannot recover an event already lost. Drop-and-count is appropriate only when loss is allowed.
</details>

### Card M6-Q3 · Recall
Q: In two sentences: what does a pre-encoded order message do at startup, and
   what is left for the hot path to do per order?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - At startup it builds the complete wire-format byte array, filling every
    field that does not change (in this example, message type and symbol).
  - Per order the hot path only copies the changing fields (price, quantity,
    order ID) into their fixed byte offsets, for example with `std::memcpy`:
    no formatting, loops or allocation at send time.
</details>

### Card M6-Q4 · Apply
Q: In code review you find `syslog(LOG_INFO, "Order sent: %d", id);` inside the
   strategy's `on_market_data` callback. (a) Name the problem in one sentence.
   (b) Describe the fix in one or two sentences.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) It formats a string and performs I/O through the kernel on the hot
    path, work whose duration your code does not bound.
  - (b) Push a small fixed-size event (order ID, price, time) into an SPSC
    ring; a separate logging thread pops it, formats the text and calls
    `syslog`. Decide in advance what happens when the ring is full (usually
    drop and count) so the hot thread never waits.
</details>

---

### CONCEPT C5  Linux only: transparent huge pages (untested here)

**Why a larger page is tempting.** A larger page covers more addresses with
one translation entry, but asking the kernel to form one can introduce work
at the very moment a thread needs memory.

This concept is Linux-specific and was not run on this course's test machine
(macOS). It is here because production trading servers usually run Linux.

**What it is.** Memory is mapped in pages. *Transparent huge pages* (THP) let
Linux back memory with much larger pages automatically, so one address-cache
(TLB) entry covers more memory. The setting
`/sys/kernel/mm/transparent_hugepage/enabled` can be `always`, `madvise`
(only regions the program marks with `madvise(..., MADV_HUGEPAGE)`) or `never`.
A program can also opt a region out with `madvise(..., MADV_NOHUGEPAGE)`.

**Where latency can come from.** The kernel guide says that with the
`defrag` setting `always`, an allocation "will stall on allocation failure and
directly reclaim pages and compact memory". Separately, a kernel thread,
`khugepaged`, collapses small pages into huge ones in the background.

**The trap.** "Huge pages are faster, so turn THP on everywhere" or the
reverse rule. Both are guesses. The guide also documents counters in
`/proc/vmstat` that count these events.

**Rule:** treat THP as a workload decision: measure tail latency and the THP
counters, then choose system-wide or per-region (`madvise`) settings.

### Card M6-G4 · Spot the trap
Q: (Linux.) After THP is set to `always` system-wide, your tail latency gets
   worse. (a) Name one THP mechanism that can stall a thread. (b) In one
   sentence, how do you decide whether to change the setting?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) With defrag enabled, an allocation can stall while the kernel
    reclaims and compacts memory to build a huge page; `khugepaged` also
    collapses pages in the background.
  - (b) Measure tail latency together with the THP counters before and after
    a change; if THP is the cause, prefer `madvise` mode (huge pages only where
    the program asks) or mark regions with `MADV_NOHUGEPAGE`.
</details>

---

## Closing

Remember: the hot path runs only bounded work on memory already touched;
expensive preparation happens at startup; slow output goes through a ring to a
cold thread. Cards you missed come back tomorrow. Next: lesson 03, how to
measure latency without fooling yourself.
