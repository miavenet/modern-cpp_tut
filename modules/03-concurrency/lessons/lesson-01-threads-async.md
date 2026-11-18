# Lesson 01 — Threads and Async: Starting Work and Waiting for It

**Module 03 adds** threads, atomics, the C++ memory model, C++20 coordination
types and coroutines. **It assumes** module 01 (lambdas, move, RAII) and a
C++20 compiler. Every runnable sample is in `modules/03-concurrency/lab/`;
the build command is on line 3 of each file.

**Hook:** an exception skips your `join()`. Does the worker leak, keep running,
or stop the program? Commit your guess before opening the answer.
**You will be able to:** start a thread safely with C++20
`std::jthread`, stop it, and predict when `std::async` really runs in the
background.

**Time:** about 24 minutes. **You need:** lambdas and RAII from
module 01.

## Pretest — commit your guess first

### Card M3-P1 · Pretest
Q: A C++11 `std::thread` object is destroyed while it still owns a thread:
   no `.join()`, no `.detach()`. What happens? Guess in one line.
<details><summary>Answer</summary>
Key:

  - The destructor calls `std::terminate()`, which ends the whole program
    (on this toolchain it prints `libc++abi: terminating`).
  - C++20 `std::jthread` does not do this: its destructor asks the thread to
    stop and then joins it.
</details>

---

## Threads: from `std::thread` to `std::jthread`

### CONCEPT C1  Owning a thread: join, terminate, and C++20 jthread

**Why this exists.** A worker can outlive the scope that launched it. If an
exception skips a hand-written join, the scope cannot safely forget the
worker. Thread ownership needs the same scope rule as file handles.

**Words first.** A *thread* is an independent line of execution inside your
process; all threads share the same memory. To *join* a thread is to wait
until it finishes. A `std::thread` object (C++11) is *joinable* from the
moment it starts a thread until you call `join()` or `detach()`.

**C++98 had no threads in the standard.** You called the operating system,
for example POSIX `pthread_create`, and joined by hand.

**C++11 `std::thread`** made threads portable, with one sharp rule:
destroying a joinable `std::thread` calls `std::terminate()`. So an
exception thrown between starting the thread and calling `join()` kills the
program.

```cpp
// C++11 fragment: the join is skipped if risky() throws -> std::terminate
void run() {
    std::thread t([] { doWork(); });
    risky();       // throws?
    t.join();
}
```

**C++20 `std::jthread`** ("joining thread", header `<thread>`) joins in its
destructor, so the exception path is safe. It also carries a
`std::stop_token` (header `<stop_token>`): a flag the owner can raise and
the worker can poll.

```cpp
// C++20 fragment (processItem is yours)
std::jthread worker([](std::stop_token st) {
    while (!st.stop_requested()) {   // poll the flag
        processItem();
    }
});
// ~jthread(): request_stop(), then join()
```

**See it yourself.** Build `lab/terminate.cpp`. It prints
`libc++abi: terminating`, maybe after `worker ran` (timing decides), and
the process aborts. Then build `lab/jthread_stop.cpp`, which uses
ThreadSanitizer (a race detector you meet properly in lesson 02). It normally
prints `worker stopped after some items`. Fine print: it can print
`zero` if stop is requested before the worker processes an item; neither
outcome indicates a data race.

**Detaching.** `t.detach()` lets the thread run on with no owner and no
join. Anything it refers to must then outlive it, which is hard to
guarantee for local variables.

**The trap.** Stopping is *cooperative*. A `jthread` whose lambda never
checks its token does not stop: the destructor's `join()` waits until the
work ends by itself.

**Rule:** own threads with `std::jthread`, and make the worker loop check
its `stop_token`.


---

### Card M3-C1 · Fix it
Q: Fill in the three blanks so this thread joins itself and can be cancelled:
   ```cpp
// Fragment: fill the blanks in this exercise.
   void backgroundProcessor() {
       std::___ worker([](std::___ st) {
           while (!st.___()) {
               processItem();
           }
       });
   }
   ```
   Answer format: one line, the three names in order, separated by commas.
<details><summary>Answer</summary>
Key:

  - `jthread, stop_token, stop_requested`
  - `jthread` joins in its destructor; `stop_token` is the flag the lambda
    receives; `stop_requested()` reads it.
</details>

### Card M3-Q1 · Recall
Q: An exception is thrown after a thread is started but before the line that
   would join it. Why does this crash with `std::thread` but not with
   `std::jthread`? Answer format: one or two sentences.
<details><summary>Answer</summary>
Key:

  - The `std::thread` is destroyed while still joinable, and its destructor
    calls `std::terminate()`.
  - The `std::jthread` destructor requests stop and joins instead, so the
    exception propagates normally.
</details>

### Card M3-G1 · Spot the trap
Q: What is wrong with this code? Answer format: one or two sentences.
   ```cpp
   void launchTask() {
       std::string data = loadConfig();   // a local variable
       std::thread t([&data] { process(data); });
       t.detach();
   }   // launchTask returns here
   ```
<details><summary>Answer</summary>
Key:

  - The detached thread can still be running after `launchTask` returns, but
    `data` is a local that is destroyed at that return. The lambda's
    reference then dangles; using it is undefined behaviour.
  - Fix: capture an owning copy (`[data]` or `[d = std::move(data)]`), or do
    not detach (join, or keep a `jthread` alive while the data is used).
</details>

---

## Tasks: `std::async` and futures

### CONCEPT C2  std::async: where the task runs and who waits

**The memory smash that waits for an exception.** In 2013 the committee was
arguing, again, over whether a future from `std::async` should wait in its
destructor. Hans Boehm called it "a particularly sensitive issue" and made his
case with one small scene.

A function owns a local `vector`, starts an async task that uses it by
reference, and calls `get()` a few lines later. Everything "may work well", he
wrote, until a line between the launch and the `get()` throws. Boehm deliberately
assumes a hypothetical `no_join_async` whose future does not wait when destroyed.
Then nothing waits: the task may run on past the exit of both functions and write into memory
that used to be the vector.

He called it a cross-thread "memory smash", "likely to be exploitable as
security holes", and subtle enough that "it is likely to be overlooked in real
code". The waiting destructor stayed, though futures from other sources do not
all block this way.

That is why the discarded future below behaves like a surprise join, and why its
lifetime belongs in your design review.

**Words first.** `std::async(policy, f)` (C++11, header `<future>`) runs
`f` and returns a `std::future`, a handle whose `.get()` waits for the
result. The *policy* says where `f` runs:

| Policy | Where `f` runs |
|---|---|
| `std::launch::async` | as if on a new thread |
| `std::launch::deferred` | later, on the thread that first waits (`get()` or `wait()`) |
| none given (both allowed) | the library picks one |

| Choose | Cost or constraint | Right when | Smell |
|---|---|---|---|
| `jthread` | Own a worker and join it | Work has a long-lived loop | A thread is started for every tiny item |
| `async` with `launch::async` | Task has a future; it may start a thread | One independent result is needed | A discarded future silently serializes calls |
| `async` with `launch::deferred` | Work waits for the first waiter | Delayed execution is acceptable | You assumed it already ran |

Advice: pick the ownership and wait point first; a hidden wait is the smell
that the chosen API does not fit the call site.

**The surprise.** For `launch::async`, releasing the last reference to an
unfinished task's shared state waits for completion. Here the discarded future
is that last reference, so the statement waits at its semicolon. A discarded
deferred task need never run.

```cpp
// C++11 fragment (slow() takes 200 ms)
std::async(std::launch::async, slow);          // waits here, at the ';'
auto f = std::async(std::launch::async, slow); // retains the future; no destruction wait here
f.get();                                       // waits here instead
```

**See it yourself.** `lab/async_block.cpp` times both statements. A typical run
prints the following. Scheduling delays can also make the second statement take
200 ms or longer:

```
statement took at least 200 ms
statement took less than 200 ms
```

Apple clang also warns on the first one: `ignoring return value of
function declared with 'nodiscard' attribute`. Read that warning as a bug
report.

**The trap.** "No policy means a new thread." No: the library may choose
`deferred`, and then nothing runs until someone waits.

**Low-latency note.** Each `std::launch::async` call behaves as if it
started a new thread. Latency-critical code usually starts its threads once
and keeps them; measure before you put `async` on a hot path.

**Rule:** pass `std::launch::async` when you need concurrency, and keep the
future alive until you want to wait.


---

### Card M3-J1 · Apply
Q: An AI assistant proposes this for **every** incoming message on a
   latency-sensitive path. Is it right here? What would you check first,
   and what evidence would make you reject it? Answer format: three short
   sentences: decision, first check, rejection evidence.
   ```cpp
   #include <future>
   int process(int x) {
       auto result = std::async(std::launch::async, [x] { return x + 1; });
       return result.get();
   }
   int main() { return process(6) == 7 ? 0 : 1; }
   ```
<details><summary>Answer</summary>
Key:

  - I would reject it for the stated hot path until measured: `get()` waits
    for this task before `process` returns, so this call gets no overlap.
  - I would first measure message rate, task duration and the distribution
    of time spent starting and waiting for tasks in the release build.
  - I would reject it if that distribution exceeds the path's latency
    budget; a long-lived worker with a queue is worth testing instead.
</details>

---

### Card M3-Q2 · Apply
Q: `std::async(std::launch::async, doWork);` is a statement on its own; the
   returned future is not stored. When does the calling thread get past
   this statement? Answer format: one sentence.
<details><summary>Answer</summary>
Key:

  - Only after `doWork` has finished. `doWork` runs on another thread, but
    the discarded future is destroyed at the end of the statement, and that
    destructor waits for the task.
</details>

### Card M3-G2 · Spot the trap
Q: You write `auto f = std::async(doWork);` with no launch policy. What does
   the standard guarantee about which thread runs `doWork`? Answer format:
   one or two sentences.
<details><summary>Answer</summary>
Key:

  - No particular thread. The library may run it as if on a new thread, or
    defer it.
  - If deferred, `doWork` runs on the thread that first calls `f.get()` or
    `f.wait()`, and not at all if nobody waits.
  - Pass `std::launch::async` when you need it to run concurrently.
</details>

---

## Closing

Remember: destroying a joinable `std::thread` means `std::terminate`; use
`jthread` and check its `stop_token`. An unstored `launch::async` future waits at
the `;`. Misses come back tomorrow. Next: lesson 02, what makes two threads
touching one variable legal or undefined.
