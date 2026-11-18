# Lesson 05 — LAB: Build a Fixed-Block Pool Allocator (PMR)

**Time: about 45 minutes** (reading about 14, the hands-on lab about 20, cards about 11). The lab is resumable: your work file stays where you left it.

**Hook:** a `std::pmr::list<Order>` never asks for an `Order`. In this lab you write a memory resource, put it under that list, and watch the upstream allocation count fall from 3000 requests to four chunk requests.

**You will be able to:** write a `std::pmr::memory_resource` subclass with a free list, measure upstream reuse after warm-up, and say which workloads keep that true.

**Needs from earlier:** lesson 01 (memory resources, upstream, placement new).

## Pretest — commit your guess FIRST (no fair reading ahead)

### Card M5-P6 · Pretest
Q: `struct Order { std::uint64_t id; double px; std::uint32_t qty; char side; };` has `sizeof(Order) == 24` here. You build a pool of 24-byte blocks for a `std::pmr::list<Order>`. Guess yes or no, plus one sentence: will the list's requests fit those blocks?
<details><summary>Answer</summary>
Key:

  - No. The list asks for *nodes*: the `Order` plus two link pointers. In this lab's run on libc++ every request was 40 bytes with alignment 8; other libraries may differ.
  - So a pool must be sized by what the container actually requests, not by `sizeof` of your element.
</details>

---

## The free list

### CONCEPT C13  A pool recycles blocks through a free list

**Same number of operations, dramatically slower.** John Lakos and three Bloomberg colleagues set out to settle an argument with numbers: do local allocators really help, or do they just complicate interfaces? Their first experiments, they admitted, "were done in some haste", so they re-ran every benchmark on dedicated machines and published the benchmark code.

The surprise: where a local allocator helped dramatically, both versions performed roughly the same number of operations. In their results, stalls from cache interactions with main memory dominated the slow global-allocator runs.

They also tried tcmalloc and jemalloc as the global allocator; the native ones did as well or better. "It isn't about how good the global allocator is," they wrote, but about local knowledge of how allocation will occur. So in this lab, count the upstream calls, and also ask whether the pool matches the real allocation pattern.

**Words first.** Lesson 01's arena does not reuse individual freed blocks until `release()`, which is fine for one message but not for an order book where orders are created and cancelled all day. A *pool* hands out *blocks* of one size and takes them back for reuse. It gets memory from upstream in big *chunks* and cuts each chunk into blocks. Free blocks are kept on a *free list*: a singly linked list whose link pointer is stored **inside each free block** (an *intrusive* list), so it needs no extra memory.

```text
free_ --> [A|next]--> [B|next]--> [C|null]       three free blocks

allocate():   take A        free_ --> B --> C       (pop the head)
deallocate(A): put A back   free_ --> A --> B --> C (push on the head)
```

Both operations touch only the head, so their cost does not depend on how many blocks exist. For requests in the bound class, the free-list fast path avoids upstream calls; refill requests a chunk. Other size/alignment classes are forwarded upstream.

**C++98 way vs modern way.**

```cpp
// fragment, C++98 style: a recycler welded to one type, invisible to std::list
Order* freeHead = 0;
Order* alloc() {
    if (!freeHead) return static_cast<Order*>(::operator new(sizeof(Order)));
    Order* o = freeHead; freeHead = *reinterpret_cast<Order**>(freeHead); return o;
}
```

In C++17 you derive from `std::pmr::memory_resource` and override three virtual functions: `do_allocate`, `do_deallocate` and `do_is_equal`. Any PMR container can then use it.

**The wrong belief:** "a pool is always constant time." The pop and push are; refill calls upstream, and that cost is not bounded by the pool.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| Monotonic arena | No reuse before whole-resource release | Short batch with one shared lifetime | Retained memory climbs during steady churn |
| Fixed-block pool | Reuses one request class; refills from upstream | Repeated same-class nodes | Different requests bypass it each round |
| General allocator | Handles mixed sizes and lifetimes | No stable class or peak | Allocation calls dominate a measured hot path |

Use the pool when reuse is real and measurable; a steady upstream climb says the class or peak assumption is wrong.

**Rule:** allocate pops the head and deallocate pushes it back. Fine print: refill and forwarded requests call upstream.


### Card M5-C6 · Fix it
Q: Fill the three blanks of the free-list core. Answer on one line: the three tokens in order, separated by commas (for example `x, y, z`). The lab writes `::new (p) node` in `push` to start the node's lifetime; the blanks are the same.
   ```cpp
// Fragment: fill the blanks in this exercise.
   struct node { node* next; };
   node* free_ = nullptr;

   void* pop()  {                    // allocate: take the head
       node* n = free_;
       free_ = n->___;               // (A)
       return n;
   }
   void  push(void* p) {             // deallocate: put it back on the head
       node* n = static_cast<node*>(p);
       n->next = ___;                // (B)
       ___ = n;                      // (C)
   }
   ```
   Answer format: one line; give blank values in order, separated by commas.

<details><summary>Answer</summary>
Key:

  - (A) `next`: the head moves to the next free block.
  - (B) `free_`: the returned block links to the old head.
  - (C) `free_`: the returned block becomes the new head.
</details>

---

## The lab

### CONCEPT C14  Do the lab: finish `fixed_pool` and prove it with a counter

**Why the counter is here.** A free list can look perfect in a code review while every real request slips past it. The container asks for node storage, not merely your element, and an unobserved size mismatch can make a pool irrelevant. The counting upstream turns that design claim into a result you can inspect after each round.

**What is in the folder** (`modules/05-memory-perf/lab/`):

- `starter/pool_lab.cpp`: compiles and runs now. It has a complete counting upstream resource, a `fixed_pool` with four `TODO(M5-C6)` markers, and a churn loop: 3 rounds of 1000 `push_back`s into a `std::pmr::list<Order>`, then `clear()`.
- `solution/pool_lab.cpp`: the reference. Open it only after your attempt.

**How the pool decides.** The first ordinary request fixes the pool’s class: that size and that alignment. Fine print: zero-byte requests are forwarded upstream without binding the pool. It rounds the block *stride* (distance between blocks) up to a multiple of the alignment, so every block is aligned. Requests outside the class go upstream unchanged, and `do_deallocate` applies the same test. The destructor returns every chunk with the size and alignment it was allocated with.

**Step 0: copy and run the starter** (from the tutorial root; the `cp` OVERWRITES an earlier work copy, so run it once):

```sh
mkdir -p modules/05-memory-perf/lab/work
cp modules/05-memory-perf/lab/starter/pool_lab.cpp modules/05-memory-perf/lab/work/
clang++ -std=c++20 -Wall -Wextra -O2 modules/05-memory-perf/lab/work/pool_lab.cpp -o "${TMPDIR:-/tmp}/pool_lab" && "${TMPDIR:-/tmp}/pool_lab"
```

<details><summary>First run prints (libc++ on the course's arm64 Mac)</summary>

```text
round 1: 1000 orders in and out, upstream allocations so far = 1000
round 2: 1000 orders in and out, upstream allocations so far = 2000
round 3: 1000 orders in and out, upstream allocations so far = 3000
pool bound to 40-byte, 8-aligned requests (block stride 40, 256 blocks per chunk); sizeof(Order) = 24
requests the pool forwarded upstream: 3000
after the pool is destroyed: upstream allocations = 3000, deallocations = 3000
RESULT: TODO - every node still goes upstream; finish the four TODO steps in fixed_pool
```
</details>

**Steps 1-4** are the `TODO(M5-C6)` comments in `lab/work/pool_lab.cpp`: (1) `refill`: one upstream chunk, recorded in `chunks_`, every block pushed; (2) `pop`; (3) `push`; (4) set `kPoolReady` to `true`. Rebuild with the same command. If it crashes, rebuild with `-g -fsanitize=address` instead of `-O2` to see where.

After editing, run the course check:
`printf '%s\n' modules/05-memory-perf/lab/work/pool_lab.cpp | modules/05-memory-perf/checks/pool_lab.check.sh`.

<details><summary>Success looks like this (same machine and library)</summary>

```text
round 1: 1000 orders in and out, upstream allocations so far = 4
round 2: 1000 orders in and out, upstream allocations so far = 4
round 3: 1000 orders in and out, upstream allocations so far = 4
pool bound to 40-byte, 8-aligned requests (block stride 40, 256 blocks per chunk); sizeof(Order) = 24
requests the pool forwarded upstream: 0
after the pool is destroyed: upstream allocations = 4, deallocations = 4
RESULT: PASS - no upstream allocations after round 1, every chunk returned
```

Four chunks of 256 blocks hold 1000 nodes; after round 1 every node comes from the free list. The count stays flat only while the most nodes alive at once fit in the blocks already fetched; a bigger peak triggers another refill. The 40 and the 4 are for this standard library; another one may request a different node size. The program exits with status 0 on PASS and 1 otherwise.
</details>

**Rule:** count upstream requests to prove reuse for the tested workload. Fine print: the bookkeeping vector allocates separately during refill, so that count does not measure every heap allocation.

### Card M5-LAB1 · Build and check
Q: Do the lab in `modules/05-memory-perf/lab/work/pool_lab.cpp`. Submit that path when your implementation is ready.
   Answer format: one path on one line.
<details><summary>Answer</summary>
Key:

  - The executable passes churn, upstream accounting, zero-size, overflow, allocation-failure and request-class checks.
</details>


### Card M5-Q11 · Recall
Q: Name the data structure that gives a pool constant-time allocate and deallocate, and say which operation on it each one is. One sentence.
   Answer format: one line.
Hint: it lives inside the free blocks themselves.

<details><summary>Answer</summary>
Key:

  - A free list (a singly linked list threaded through the free blocks): allocate pops the head, deallocate pushes onto the head, with no searching.
</details>

### Card M5-Q12 · Apply
Q: You reuse the unchanged `fixed_pool` to recycle FIX message buffers: every request is 512 bytes with alignment 8. After warm-up, will it stop calling upstream? Answer yes or no, plus the conditions that must hold.
   Answer format: one line.
Stretch: What happens if some buffers are 512 bytes and some 4096 bytes, on the same pool?

<details><summary>Answer</summary>
Key:

  - Yes, if every ordinary request has the same size and alignment as the first ordinary one (512 bytes, alignment 8) and the pool already holds enough blocks for the peak number of live buffers; a higher peak triggers another refill. Zero-byte requests go upstream without binding the pool.
</details>
<details><summary>Stretch answer</summary>
StretchKey:
  - The first ordinary request binds the pool; every request of the other size goes upstream each time. Zero-byte requests go upstream without binding the pool. Use one pool per size class, or `std::pmr::unsynchronized_pool_resource`, which keeps several pools for different block sizes.
</details>

---

## Two traps

### CONCEPT C15  Element size is not request size; one pool, one class

**Why this trap survives tests.** A test that creates only one element type can hide a pool's single-class limit. Add a second list and nothing necessarily crashes; the upstream counter just starts moving again. That quiet fallback is why the request's size *and alignment* belong in the decision.

**Trap 1: sizing by the element.** A container allocates its own nodes, and their size is the library's business: 40 bytes for `std::pmr::list<Order>` on the shown libc++ build, not 24. A pool that hands out 24-byte blocks for a 40-byte request lets the list write past the block: undefined behaviour. Bind to the actual request, and check it in release builds too: `assert` disappears under `-DNDEBUG`, so use a real test that forwards to upstream or throws `std::bad_alloc`.

**Trap 2: one fixed pool, two node sizes.** Put a second list with a different element type on the same `fixed_pool` and its requests are outside the class. Those requests go upstream each time; the count can keep climbing.

**The standard's answer.** `std::pmr::unsynchronized_pool_resource` (C++17) keeps pools for several block sizes and can reuse blocks in each class. Large requests can be forwarded upstream, and growing demand can trigger new chunks. It is not thread-safe; `synchronized_pool_resource` is.

| Choice | Cost and guarantee | Good fit | Wrong-choice smell |
|---|---|---|---|
| One `fixed_pool` | Reuses its first ordinary size/alignment class; other requests forward | One known node request | Counter rises with a second container type |
| One fixed pool per class | More resources to manage; each class can reuse blocks | A few known request classes | New classes keep appearing in production |
| `unsynchronized_pool_resource` | Manages several block sizes; requires external thread confinement or synchronisation | Mixed sizes on one thread | Threads touch the same resource concurrently |

Start with the request trace, then choose the simplest pool arrangement that covers those classes.

**Rule:** bind a fixed pool to the request the container really makes, and give each size class its own pool.


### Card M5-G13 · Spot the trap
Q: You size the pool's blocks to `sizeof(Order)` (24 bytes) and put it under a `std::pmr::list<Order>`. Release builds show corrupted orders. (a) What did you get wrong? (b) What is the fix? One sentence each.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) The list requests nodes larger than `Order` (40 bytes on libc++ here), so each node overruns its 24-byte block: undefined behaviour.
  - (b) Bind the pool to the size and alignment actually requested in `do_allocate` (as the lab does), and send anything else upstream or throw `std::bad_alloc`; an `assert` alone vanishes in release builds.
</details>

### Card M5-G14 · Spot the trap
Q: An AI assistant replaced two fixed pools with one standard pool for both lists, saying it "handles both sizes with no upstream calls after warm-up". The matching thread fills `orders`; a separate audit thread fills `notes` at the same time. Is this right here? What would you check first, and what would change your verdict?
   ```cpp
   // Fragment: counting_resource and Order are the lab's;
   // LargeNote is a separately declared 200-byte test type.
   std::pmr::unsynchronized_pool_resource pool(&upstream);
   std::pmr::list<Order> orders(&pool);       // matching thread
   std::pmr::list<LargeNote> notes(&pool);    // audit thread
   ```
   Answer format: one line; separate (a), (b), and (c) with semicolons.

<details><summary>Answer</summary>
Key:

  - (a) Reject the concurrent use: both threads allocate through one unsynchronized resource. Supporting both request sizes does not by itself guarantee zero upstream calls after warm-up.
  - (b) Check resource ownership, the size/alignment requests, the pooling threshold, and peak live allocations.
  - (c) On one thread, this can be appropriate if the requests are pooled and warmed capacity covers the workload. With concurrent users, use a synchronized resource or separate resources. If separate resources share the lab's counting upstream, make its counters thread-safe or give each thread its own upstream counter.
</details>

---

## Integrate (one synthesis question)

### Card M5-INT2 · Choose and explain
Q: An order-book engine has three parts: (1) a scratch area for parsing one market-data message, dead when `on_message` returns; (2) the resting-order store, with orders created and cancelled all day; (3) a scan that sums resting quantity over all price levels on every update. For each, choose arena, pool, or SoA layout, and justify it in one sentence by lifetime or access pattern.
   Answer format: one line.

<details><summary>Answer</summary>
Key:

  - (1) Arena (`monotonic_buffer_resource` over a local buffer): everything dies together at the end of the call, so nothing needs individual reuse; pass `null_memory_resource()` upstream if arena fallback must be forbidden.
  - (2) Pool (`fixed_pool` or `unsynchronized_pool_resource`): constant churn needs blocks reused; bind to the container's real request size and give each size class its own pool.
  - (3) SoA: the scan reads one field of every level, so storing quantities contiguously moves fewer bytes per cache line than whole structs; measure it.
  - The idea underneath: lifetime picks the allocator, access pattern picks the layout.
</details>

---

## Close

**Remember:** pop and push the head; prove upstream reuse with a counter; one fixed pool serves one size class. **Tomorrow:** missed cards come back. **Next:** lesson 06 puts this pool under a latency histogram (the LPA lens).
