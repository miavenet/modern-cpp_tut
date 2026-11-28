# Lesson 02 — Smart pointers: put the owner in the type

An order pointer goes to risk, logging and execution. Who calls `delete`? In
C++98 the answer lived in a comment, and when the comment was wrong you got a
leak or a double delete. By the end of this lesson you can pick `unique_ptr`,
`shared_ptr` or `weak_ptr` for an ownership situation, hand ownership along a
pipeline with `std::move`, and spot the three classic smart-pointer bugs.

**Time:** about 29 minutes. **You need:** lesson 01 (rvalues, `std::move`,
moved-from objects).

## Pretest — guess first

### Card M1-P2 · Pretest
Q: Guess. In C++98 you `new` an `Order` and give the pointer to three
   components (risk, logging, execution). Who should call `delete`? What
   happens if two of them do? How might C++11 put the answer into the code?
   One or two sentences.

<details><summary>Answer</summary>
Key:
  - C++98: nothing in the code says who owns it; the team must agree by
    convention. Two `delete` calls on one pointer is undefined behaviour (often a
    crash, sometimes silent heap damage); no `delete` is a leak.
  - C++11 adds smart pointers that state ownership in the type: `unique_ptr`
    (exactly one owner), `shared_ptr` (several owners, counted), `weak_ptr` (an
    observer that owns nothing). The object is deleted automatically when its
    last owner goes away.
</details>

---

## One owner: `unique_ptr`

### CONCEPT C5  `unique_ptr`: exactly one owner, handed on with `std::move`

**The copy that emptied its source.** In C++98, `std::auto_ptr<Order> b = a;`
looked like a copy. It was not: `b` took the order and `a` was left empty.

Generic code had no way to tell. As the paper that replaced it concluded,
"generic code is likely to initiate a move when a copy was intended", so
`auto_ptr` "moves from lvalues using copy syntax and is thus fundamentally
unsafe."

The fix, accepted at Oxford in 2007, was `unique_ptr`: the same single owner,
but a transfer must be spelled as a move, and `auto_ptr` was deprecated. By
2014 Howard Hinnant said he would like to see `auto_ptr` removed altogether.

So at each call boundary the question is now visible: does this function
merely use the object, or does ownership leave the caller?

**The C++98 way.** `Order* o = new Order(100);` and every early `return` or
exception between `new` and `delete` is a leak, unless you remember the
`delete` on each path.

**The modern way.** `std::unique_ptr<Order>` (C++11) owns one object and deletes
it in its destructor, so every exit path cleans up. It cannot be copied (there
would be two owners), only moved. Moving it transfers ownership: the destination
takes the pointer and the source is guaranteed to become null.
`std::make_unique<Order>(100)` (C++14) creates both in one step. In C++11 write
`std::unique_ptr<Order> o(new Order(100));` instead.

```cpp
#include <cstdio>
#include <memory>
#include <utility>

struct Order {
    int qty;
    explicit Order(int q) : qty(q) {}
    ~Order() { std::printf("~Order(%d)\n", qty); }
};

void execute(std::unique_ptr<Order> o) { std::printf("executing %d\n", o->qty); }

int main() {
    auto order = std::make_unique<Order>(100);   // C++14 factory
    execute(std::move(order));                   // ownership moves into execute
    std::printf("order is %s\n", order ? "set" : "null");
    std::printf("sizeof(unique_ptr<Order>)=%zu sizeof(Order*)=%zu\n",
                sizeof(std::unique_ptr<Order>), sizeof(Order*));
}
```

**Predict first:** in which order do the lines print, and when does `~Order`
run?

<details><summary>Output (Apple clang 21, arm64)</summary>

```
executing 100
~Order(100)
order is null
sizeof(unique_ptr<Order>)=8 sizeof(Order*)=8
```

`~Order` runs when `execute`'s parameter goes out of scope, because `execute`
became the owner. With the default deleter the `unique_ptr` is the size of a raw
pointer on this toolchain; a custom deleter with state can make it bigger.
</details>

**The trap.** "The compiler stops you using a moved-from `unique_ptr`" is false.
After `execute(std::move(order));`, the line `return order->qty;` compiles with
no warning under `-Wall -Wextra`. Dereferencing the null pointer is undefined
behaviour; no particular diagnostic or runtime outcome is guaranteed.
Testing `if (order)`, `reset()` and assigning a new pointer
are all fine.

**Rule:** a `unique_ptr` means one owner; pass ownership with `std::move`, and
treat the source as null afterwards, because nothing stops you dereferencing it.


### Card M1-C2 · Fix it
Q: Refactor this C++98 code so no `delete` is needed and `orderBook` ends up
   as the only owner.
   ```cpp
   // Fragment: Order, riskCheck and orderBook are your own code.
   Order* order = new Order(symbol, qty, price);
   if (!riskCheck(*order)) {
       delete order;            // easy to forget on one path
       return false;
   }
   orderBook.add(order);        // who owns it now?
   ```
   Three parts, one line each: (a) the new first line (C++14), (b) the new
   `add` call, (c) why `unique_ptr` and not `shared_ptr`.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `auto order = std::make_unique<Order>(symbol, qty, price);` (C++11:
    `std::unique_ptr<Order> order(new Order(symbol, qty, price));`). The
    `delete` line disappears: leaving the function destroys the order.
  - (b) `orderBook.add(std::move(order));` with `add` taking
    `std::unique_ptr<Order>` by value, so the book becomes the owner.
  - (c) There is one owner at any moment (this function, then the book), so
    shared ownership and its reference count are not needed.
</details>

### Card M1-INT2 · Choose and explain
Q: Two parts, one or two sentences each.
   (a) An order travels parser -> risk -> matching as a `unique_ptr`, with
   `std::move` at each hand-off. What happens to the pointer in the stage that
   handed it on?
   (b) What does the language guarantee, and what does it not guarantee, if a
   stage uses its pointer after handing it on?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Ownership moves to the next stage and the previous stage's
    `unique_ptr` becomes null. No `Order` is copied; only the pointer changes
    hands, and exactly one stage owns the order at any time.
  - (b) Guaranteed: the moved-from pointer is null. Not guaranteed: any error.
    Dereferencing it compiles and is undefined behaviour because the pointer is
    null. Static analysers can warn (clang-tidy has a
    `bugprone-use-after-move` check), but the compiler does not prevent it.
</details>

---

## Shared owners and observers: `shared_ptr`, `weak_ptr`

### CONCEPT C6  `shared_ptr` counts owners; `weak_ptr` watches without owning

**When one owner is not enough.** Sometimes several components must keep an
object alive and none of them knows who finishes last. `std::shared_ptr`
(C++11) keeps separate ownership bookkeeping, often allocated together with
the object by `make_shared`, holding a *use count*: the
number of `shared_ptr`s that own it. Copying a `shared_ptr` adds one; destroying
or resetting one subtracts one; at zero the object is deleted.

**`weak_ptr`.** A `std::weak_ptr` (C++11) refers to the same control block but
is not counted as an owner. `lock()` gives you a `shared_ptr` if the object
still exists, or an empty one if it has gone.

```cpp
#include <cstdio>
#include <memory>

struct Feed { ~Feed() { std::puts("~Feed"); } };

int main() {
    auto feed = std::make_shared<Feed>();
    std::printf("owners: %ld\n", feed.use_count());
    {
        std::shared_ptr<Feed> risk = feed;      // copy: one more owner
        std::shared_ptr<Feed> logger = feed;
        std::printf("owners: %ld\n", feed.use_count());
    }                                           // risk and logger destroyed
    std::printf("owners: %ld\n", feed.use_count());
    std::weak_ptr<Feed> watcher = feed;         // observes, does not own
    std::printf("owners: %ld\n", feed.use_count());
    feed.reset();                               // last owner gone
    std::printf("expired: %s\n", watcher.expired() ? "yes" : "no");
    if (auto locked = watcher.lock()) std::puts("still alive");
    else std::puts("lock() returned empty");
}
```

**Predict first:** the four owner counts, and where `~Feed` prints.

<details><summary>Output (Apple clang 21, libc++)</summary>

```
owners: 1
owners: 3
owners: 1
owners: 1
~Feed
expired: yes
lock() returned empty
```
</details>

**What it costs.** A `shared_ptr` here is 16 bytes (object pointer plus
control-block pointer). The count must be safe to change from several threads,
so libraries typically update it with *atomic* instructions: an increment that
other CPU cores see as one indivisible step. These cost more than a plain
increment, especially when many cores touch the same count.

Creating with
`std::make_shared` typically puts object and control block in one allocation;
`shared_ptr<T>(new T)` needs two. Counting `operator new` calls on this
toolchain gave 1 and 2.

**Passing it around.** A function that only uses the object during the call
does not need to own it: give it `const Feed&` or `Feed*`, and the caller's
`shared_ptr` keeps the object alive meanwhile.

**Two traps.** Every `shared_ptr` built from a raw pointer gets a new control
block, so building two from one raw pointer gives two counts and two deletes;
copy the first `shared_ptr` instead. And two objects owning each other never
reach zero: that is a *reference cycle*, and both leak. Make one direction a
`weak_ptr`.

**Rule:** use `shared_ptr` only when ownership is really shared, use
`weak_ptr` for anything that should observe without keeping the object alive.


| Parameter | Can the callee retain ownership? | Cost or obligation | Right when |
|---|---|---|---|
| `const T&` | No | Caller must keep it alive | Non-null, read-only use during the call |
| `T*` | No | Check null if allowed; caller owns lifetime | Optional non-owning access |
| `unique_ptr<T>` by value | Yes, by moving it into retained storage | Caller gives up ownership | Callee becomes sole owner |
| `shared_ptr<T>` by value | Yes, by retaining an owning copy | Copying updates the count; moving transfers the handle | Callee may keep it independently |

Advice: make the parameter describe who must keep the object alive; a
`shared_ptr` copied at every call is a smell when the callee only reads it.

### Card M1-J2 · Choose and explain
Q: `queueAudit` runs on the order path. An audit thread reads the queue later,
   after the caller has dropped its owner. A teammate quotes "no `shared_ptr` by value
   on a hot path" and wants `const Order&`. Is the AI-written by-value
   `shared_ptr` right here? What would you check first?
   Assume queue access and publication to the audit thread are synchronized,
   and the order is not mutated after publication.
   Answer format: three short sentences.
   ```cpp
   #include <cstdio>
   #include <memory>
   #include <utility>
   #include <vector>
   struct Order { int quantity; };
   std::vector<std::shared_ptr<const Order>> auditQueue;   // drained later by the audit thread
   void queueAudit(std::shared_ptr<const Order> order) {
       auditQueue.push_back(std::move(order));
   }
   int main() {
       auto owner = std::make_shared<Order>(Order{100});
       queueAudit(owner);
       owner.reset();                                       // the caller is finished with it
       std::printf("%d %ld\n", auditQueue[0]->quantity, auditQueue[0].use_count());
   }
   ```

<details><summary>Answer</summary>
Key:
  - Keep the by-value `shared_ptr` here: the queue stores it, so the function really is an owner, and the order survives `owner.reset()` (the program prints `100 1`).
  - First check whether the callee keeps the object after it returns, not whether the path is hot.
  - If the audit copied the fields it needs during the call, `const Order&` would be right and cheaper; a pointer taken from that reference and queued would dangle after the reset.
  - The owning pointer solves lifetime; synchronization must separately protect
    the queue and publication.
</details>

### Card M1-Q3 · Recall
Q: Three parts, one line each. For each smart pointer, give its ownership
   model and one situation where you would use it.
   (a) `unique_ptr`  (b) `shared_ptr`  (c) `weak_ptr`
   Answer format: one line; separate labeled parts with semicolons.
Hint: Ask who keeps the object alive.


<details><summary>Answer</summary>
Key:
  - (a) Exactly one owner; move-only. The default, for example an order
    handed along a pipeline.
  - (b) Several owners counted in a control block; the object dies with the
    last one. For example a feed object that several components keep alive.
  - (c) Owns nothing; observes a `shared_ptr`-owned object and must `lock()`
    before use. For example a cache or a back pointer that must not keep the
    object alive or create a cycle.
</details>

### Card M1-Q4 · Apply
Q: A session manager creates a `TcpConnection` used by a `HeartbeatMonitor`
   and a `MessageRouter`; the connection must live while either still uses it.
   The monitor also keeps a list of recent connections for logging, which must
   not keep any of them alive. Two parts, one line each: which smart pointer
   for (a) the monitor's and router's handle, (b) the logging list? Why?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `std::shared_ptr<TcpConnection>` in both: they share ownership and the
    connection dies when the last of them lets go.
  - (b) `std::weak_ptr<TcpConnection>`: it observes without owning, so releasing
    the last owning `shared_ptr` can destroy the connection and the entry
    reports `expired()`. A `shared_ptr` in the
    list would keep each connection alive until its entry is erased. That is
    not a cycle, just an extra owner.
</details>
Stretch: The router is on a hot path and copying `shared_ptr`s there costs
  count updates. What can it hold instead?
<details><summary>Optional extension answer</summary>
StretchKey:
  - A plain reference or raw pointer that does not own, plus a documented
    rule that an owning `shared_ptr` outlives the router's use of it. No count
    is updated, but the compiler no longer checks the lifetime for you.
</details>

### Card M1-G4 · Spot the trap
Q: Two parts, one line each.
   ```cpp
   // Fragment: needs <memory>.
   struct Node {
       std::shared_ptr<Node> next;
       std::shared_ptr<Node> prev;
   };
   // a->next = b; b->prev = a;   then a and b go out of scope
   ```
   (a) Neither node's destructor ever runs. What is this bug called?
   (b) Which one-word type change fixes it, and on which member?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) A reference cycle: `a` owns `b` and `b` owns `a`, so neither use count
    reaches 0 and both leak.
  - (b) Make the back pointer non-owning: `std::weak_ptr<Node> prev;`. Then
    both destructors run when the scope ends.
</details>

### Card M1-G5 · Spot the trap
Q: Two parts, one sentence each.
   ```cpp
   // Fragment: Order has a constructor taking (const char*, int, double).
   Order* raw = new Order("AAPL", 100, 150.0);
   std::shared_ptr<Order> sp1(raw);
   std::shared_ptr<Order> sp2(raw);
   ```
   (a) What goes wrong when `sp1` and `sp2` are destroyed, and why?
   (b) Give a correct version.

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Each constructor from a raw pointer makes its own control block, so
    both counts are 1 and both call `delete raw`: a double delete, which is
    undefined behaviour (often a crash).
  - (b) Create once and copy: `auto sp1 = std::make_shared<Order>("AAPL", 100,
    150.0); auto sp2 = sp1;` (one control block, use count 2).
</details>

### Card M1-G6 · Spot the trap
Q: A colleague passes `std::shared_ptr<Order>` by value to every function on a
   hot path, none of which keeps the order after returning. Two parts, one
   sentence each: (a) what work does each call add? (b) what should those
   functions take instead?

   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Passing an lvalue by value copies the `shared_ptr`; passing an rvalue
    can move it. Copying updates the use count, typically with atomic
    instructions. Measure on your workload before assuming how much it costs.
  - (b) `const Order&` (or `Order*` if it may be null), since the caller's
    `shared_ptr` keeps the order alive during the call. Pass an owning pointer
    when the callee must participate in lifetime, including retaining ownership
    or keeping the object alive through a reentrant call.
</details>

---

> Extra practice (optional): Cards M1-PB10 and M1-PB11 in lesson-06-practice-bank-03-growth-sharing.

---

## Remember

- `unique_ptr` is the default: one owner, moved along, null afterwards (and
  nothing stops you dereferencing it).
- `shared_ptr` counts owners; `weak_ptr` observes; a cycle of owners leaks.
- Use `T&` or `const T&` for non-null borrowing, and `T*` for optional
  borrowing; use `const` for read-only access.

Missed cards come back tomorrow. Next: lesson 03, `auto` and lambdas.
