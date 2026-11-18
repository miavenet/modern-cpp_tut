# Lesson 03 — Waiting, Sharing and Coordinating: Mutexes, Spinlocks, Cache Lines, Latches

**Hook:** two threads update different counters, yet their cache lines may
force the cores to compete. Can you prove the counters share a line? **You will be able to:** pick between a
mutex and a spinlock, find false sharing, start workers with a C++20 latch,
run phases with a barrier, and make a plain `int` atomic in place.

**Time:** about 29 minutes. **You need:** lesson 02 (data race,
release/acquire).

## Pretest — commit your guess first

### Card M3-P3 · Pretest
Q: A table is read 99% of the time by many threads and written 1% of the
   time. Does `std::shared_mutex` (C++17, many readers or one writer) always
   give more read throughput than `std::mutex`? Guess yes or no, and why,
   in one line.
<details><summary>Answer</summary>
Key:

  - No, not always. Readers do run at the same time, but in common
    implementations every reader's lock and unlock still writes the lock's
    shared state.
  - Reader locking can contend on shared synchronization state. Longer read
    sections may make concurrent reading worthwhile, but compare against a
    mutex using the actual workload.
</details>

---

## Waiting for a lock

### CONCEPT C7  Mutex or spinlock: what waiting costs

**The lander that kept rebooting.** Mars Pathfinder was on the surface,
sending back more data than anyone had planned for, and its computer kept
resetting itself. Data already collected survived, but after a reset "the
remainder of the activities for that day were not accomplished until the next
day."

On Earth, Glenn Reeves's team ran "the same set of activities over and over
again in the lab" and caught it in under 18 hours. With the trace tool they had
kept in the flight software, "the priority inversion problem was obvious".

`bc_dist`, the high-priority task that collected the bus data, was waiting on a
mutex held by the "much lower priority" weather-science task, ASI/MET.
Medium-priority tasks kept the holder off the CPU, `bc_dist` missed its
eighth-of-a-second cycle, and the scheduler task declared the error that reset
the computer.

The fix was one flag: create that semaphore with priority inheritance, so the
holder runs long enough to release it. After extensive testing they sent only
the difference to Mars, where onboard software patched its own copy.

The lesson for a lock on a deadline path: look at who can hold it and what can
preempt that holder, because the lock call alone does not tell you the worst
wait.

**Words first.** A *critical section* is code that only one thread may run
at a time. A *mutex* (C++11 `std::mutex`, header `<mutex>`) guards one.
When it is already held, the waiting thread may be *blocked*: the operating
system stops running it until the mutex is free. Implementations may spin
briefly before blocking. A *spinlock* never blocks: the waiter loops,
asking again and again.

**C++98 → C++11.** C++98 used `pthread_mutex_t` by hand. C++11 gives
`std::mutex` with RAII guards: `std::lock_guard<std::mutex> g(m);` unlocks
at the end of the scope, even on an exception.

**A spinlock done properly: `lab/spinlock.cpp`** (prints
`total = 200000`, TSan silent).

```cpp
// C++20 fragment; cpu_relax() is defined in the lab file
class Spinlock {
    std::atomic_flag flag_;                    // C++20: starts clear
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) // try to take it
            while (flag_.test(std::memory_order_relaxed))      // C++20: read only
                cpu_relax();                                   // spin hint
    }
    void unlock() { flag_.clear(std::memory_order_release); }
};
```

`cpu_relax()` is the CPU's *spin-wait hint*. On x86 it is `_mm_pause()` (the
PAUSE instruction, header `<immintrin.h>`, x86 only: on arm64 that header
stops with `This header is only meant to be used on x86 and x64
architecture`). On arm64, clang's `__builtin_arm_yield()` emits YIELD. The
lab file picks one with `#if`. Standard C++ has no portable spelling.

**When a spinlock wins, and when it hurts.** It avoids the cost of
blocking and waking when waits are very short and each thread has its own
core. It burns a core while waiting. If the holder is descheduled by the
OS, every waiter spins uselessly until it runs again.

| Choose | Cost or guarantee | Right when | Smell |
|---|---|---|---|
| Mutex | Waiter may block; guard unlocks by scope | Waiting is acceptable | Held across a deadline-critical operation |
| Spinlock | Waiter burns CPU until lock is free | Measured tiny waits with runnable holder | Holder can be descheduled while others spin |
| Single owner plus messages | Needs a hand-off design | One worker can own mutable state | Several threads still mutate the same state |

Advice: measure wait tails and holder scheduling before choosing a lock;
a stalled holder is the smell that a spinlock is the wrong shortcut.

**The trap.** `std::this_thread::yield()` is not the CPU hint: it asks the
OS scheduler to run something else.

**Rule:** choose by measured wait times, holder scheduling and contention; on a
latency-critical path that must never block, use non-blocking designs
rather than a contended mutex.


---

### Card M3-J3 · Apply
Q: `reload()` runs on a low-priority admin thread. The quote thread, which has
   a deadline, calls `currentVersion()` on every quote. An AI assistant guarded
   both with one mutex. Is it right here? What would you check first, and what
   would change your answer? Answer format: three short sentences:
   decision, first check, rejection evidence.
   ```cpp
   #include <cstdio>
   #include <mutex>
   int config_version = 0;
   std::mutex config_mutex;
   void reload() {                      // low-priority admin thread
       std::lock_guard<std::mutex> guard(config_mutex);
       ++config_version;
   }
   int currentVersion() {               // quote thread, every quote, on a deadline
       std::lock_guard<std::mutex> guard(config_mutex);
       return config_version;
   }
   int main() { reload(); std::printf("%d\n", currentVersion()); }
   ```
<details><summary>Answer</summary>
Key:

  - The code is race-free, but the deadline alone is insufficient to approve
    or reject it. Measure lock-wait tails during reloads and preemption
    against the actual budget.
  - For this independent integer, the tested toolchain's lock-free
    `atomic<int>` removes the mutex wait; it does not guarantee a wall-clock deadline.
</details>

---

### Card M3-Q6 · Apply
Q: An order-book update path has a sub-microsecond budget, and several
   threads update the book. Name one way a contended `std::mutex` can blow
   that budget, and one way a naive spinlock can also blow it.
   Answer format: two short lines, (a) mutex and (b) spinlock.
<details><summary>Answer</summary>
Key:

  - (a) A waiter may be blocked by the OS, and waking it again can take far
    longer than the budget (implementations may spin first, but blocking is
    allowed).
  - (b) If the lock holder is descheduled, waiters spin and burn their cores
    until it runs again. Decide by measuring, or redesign so only one
    thread owns the book.
</details>

### Card M3-G6 · Spot the trap
Q: One spinlock repeatedly calls `test_and_set`; another polls with relaxed
   `test()` between attempts. Why might the second reduce contention, and why
   does neither protect your deadline if the holder is descheduled?
   Answer format: two sentences.
<details><summary>Answer</summary>
Key:

  - Read-only polling avoids repeatedly attempting a read-modify-write while
    the lock is held. Both still depend on the holder running and releasing
    the lock, so measure waiting under realistic scheduling.
</details>

---

## Cache lines and false sharing

### CONCEPT C8  Cache lines: why separate variables can still collide

**Why this exists.** The source says two counters are separate, but cache
coherence moves memory in lines. Distinct variables can still make two
cores compete for the same line.

**Words first.** A CPU core keeps copies of memory in its *cache*, in
fixed-size blocks called *cache lines*. When a core needs ownership to write a line,
other cores' copies of that line may be invalidated. Repeated writes by
the same owner need no ownership transfer. When cores keep writing the same line, it is passed back and
forth: *cache-line bouncing*.

- *True sharing*: threads write the same variable and may compete for its line.
- *False sharing*: threads write different variables that happen to share
  a line, so it bounces anyway.

**Sizes on the tested toolchain.** Cache-line size and C++17's
`std::hardware_destructive_interference_size` (header `<new>`) may differ.
Check both on your target before choosing padding.

**See it yourself: `lab/false_sharing.cpp`** (built with `-O2`).

```cpp
// excerpt of lab/false_sharing.cpp
struct Packed { std::atomic<long> a{0}; std::atomic<long> b{0}; };          // 16 bytes
struct Padded { alignas(128) std::atomic<long> a{0};
                alignas(128) std::atomic<long> b{0}; };                     // 256 bytes
// thread 1 increments a, thread 2 increments b, 20 million times each
```

Compare packed and padded counters under the same workload. Their addresses
and the target cache-line size tell you whether false sharing is plausible.

**The trap.** "No variable is shared, so there is no contention."
Contention is per cache line, not per variable.

**Rule:** give variables written by different threads their own cache
line (`alignas` with your target's line size), and measure.


---

### Card M3-G5 · Spot the trap
Q: Two unrelated counters are each updated heavily by a different thread:
   ```cpp
   struct Counters {
       std::atomic<int> thread1_count;
       std::atomic<int> thread2_count;
   };
   ```
   Performance is poor. Name a plausible cause, explain how to verify it,
   and propose an experiment. Answer format: three short lines.
<details><summary>Answer</summary>
Key:

  - False sharing is a candidate. Check the counters' addresses against the
    target's cache-line size, then compare with separated counters under the
    same workload. When competing cores need ownership of the shared line,
    it can bounce between them despite the counters being different objects.
</details>

---

## Coordinating a group of threads

### CONCEPT C9  std::latch and std::barrier (C++20)

**Why this exists.** A one-time start gate and a repeated phase boundary
look similar until the second round. Reusing a one-shot counter would
require rebuilding it and proving no thread still touches the old one.

**Words first.** Both make threads wait for each other, without a hand-made
mutex plus condition variable.

| Type | Header | Counts down | Reusable? |
|---|---|---|---|
| `std::latch` | `<latch>` | once, to zero | no |
| `std::barrier` | `<barrier>` | each phase, then resets | yes, and runs an optional completion function per phase |

**See it yourself: `lab/latch_barrier.cpp`** (TSan silent).

```cpp
// excerpt of main() in lab/latch_barrier.cpp
constexpr int kWorkers = 3;
int ready_flags[kWorkers] = {};             // each worker writes its own slot
std::latch started(kWorkers);               // counts down from 3, once
int phase = 0;                              // touched only by the completion step
std::barrier sync(kWorkers, [&phase]() noexcept {
    std::printf("phase %d complete\n", phase++);   // once per phase
});

std::vector<std::jthread> workers;
for (int id = 0; id < kWorkers; ++id)
    workers.emplace_back([&, id] {
        ready_flags[id] = 1;                // "initialise"
        started.count_down();               // this worker is ready
        for (int round = 0; round < 2; ++round)
            sync.arrive_and_wait();         // wait for all 3, then reuse
    });

started.wait();                             // until the count reaches 0
std::printf("all %d workers initialised\n",
            ready_flags[0] + ready_flags[1] + ready_flags[2]);
```

It prints `all 3 workers initialised`, `phase 0 complete` and
`phase 1 complete`; the first line may come before or after `phase 0`,
because the workers do not wait for main. Reading `ready_flags` after
`wait()` is safe: each `count_down()` happens before `wait()` returns.

**The trap.** Expecting a latch to reset for the next round. It cannot;
that is what a barrier is for.

**Rule:** latch for one-time start-up, barrier for repeated phases.


---

### Card M3-Q5 · Recall
Q: What is the difference between `std::latch` and `std::barrier`?
   Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - `std::latch` counts down once; after it reaches zero it cannot be reset
    or reused.
  - `std::barrier` is reusable: when all participants arrive, it runs its
    optional completion function and starts the next phase.
</details>

---

## Atomic access to existing data

### CONCEPT C10  std::atomic_ref (C++20): atomic operations on a plain object

**The problem.** A struct you cannot change has a plain `int`. You want
threads to update it atomically. `std::atomic<int>` would change the type.
`std::atomic_ref<int>` (C++20, header `<atomic>`) gives atomic operations
on an existing object without changing its declared type.

The design exists for code whose object layout is fixed. Its convenience
does not make a plain access elsewhere safe; every participant still has
to follow the atomic protocol.

```cpp
#include <atomic>
#include <cstdio>
struct LegacyData { int counter; };
int main() {
    static_assert(std::atomic_ref<int>::required_alignment == alignof(int));
    LegacyData data{};                         // {} : counter starts at 0
    std::atomic_ref<int> atomic_counter(data.counter);
    atomic_counter.fetch_add(1, std::memory_order_relaxed);
    std::printf("counter=%d\n",
                atomic_counter.load(std::memory_order_relaxed)); // counter=1
}
```

**Two conditions.** The object must be aligned to
`std::atomic_ref<T>::required_alignment`; for `int` here it is 4, the same
as `alignof(int)`, which is why the `static_assert` passes. For other types
it can be stricter, so check it. While any `atomic_ref` refers to the object,
every access must go through an `atomic_ref`. Plain access is allowed again
after all those references are gone.

**The trap.** Wrapping it in `atomic_ref` in one thread while another
thread still uses the plain `data.counter`: that is still a data race.

**Rule:** use `atomic_ref` for objects whose type you cannot change, check
`required_alignment`, and route every access through it while it exists.


---


### Card M3-C3 · Fix it
Q: Fill in the blank to update a plain `int` atomically without changing
   the struct (C++20):
   ```cpp
   struct LegacyData {
       int counter; // cannot change this struct
   };

   LegacyData data{};
   std::___<int> atomic_counter(data.counter);
   atomic_counter.fetch_add(1, std::memory_order_relaxed);
   ```
   Answer format: one name.
<details><summary>Answer</summary>
Key:

  - `atomic_ref`
  - While any `atomic_ref` refers to the object, every access must go through
    an `atomic_ref`. Plain access is allowed again after all those references
    are gone. Check `required_alignment` first.
</details>

---

## Closing

Remember: a mutex may block, a spinlock burns a core, and you choose by
measuring. Contention is per cache line. Latch once, barrier per phase.
Misses come back tomorrow. Next: lesson 04, coroutines.
