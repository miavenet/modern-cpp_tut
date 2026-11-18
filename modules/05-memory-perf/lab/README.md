# Pool allocator lab

Copy `starter/pool_lab.cpp` to `work/pool_lab.cpp` from the tutorial root:

```sh
mkdir -p modules/05-memory-perf/lab/work
cp modules/05-memory-perf/lab/starter/pool_lab.cpp modules/05-memory-perf/lab/work/
```

The copy command overwrites earlier work. Edit the four `TODO(M5-C6)` markers,
then compile and run:

```sh
clang++ -std=c++20 -Wall -Wextra -O2 modules/05-memory-perf/lab/work/pool_lab.cpp -o "${TMPDIR:-/tmp}/pool_lab" && "${TMPDIR:-/tmp}/pool_lab"
printf '%s\n' modules/05-memory-perf/lab/work/pool_lab.cpp | modules/05-memory-perf/checks/pool_lab.check.sh
```

The starter prints `RESULT: TODO`; the completed lab prints `RESULT: PASS`.
The check also tests zero-size requests, overflow, failed chunk bookkeeping,
and a second alignment class. Its upstream count measures requests to that
resource; the bookkeeping vector may allocate separately.
The separate `solution/` folder contains the reference after your attempt.

## Lesson 06: the pool's tail (LPA lens)

After `work/pool_lab.cpp` passes, copy the tail lab next to it and run it:

```sh
cp modules/05-memory-perf/lab/starter/pool_tail.cpp modules/05-memory-perf/lab/work/
clang++ -std=c++20 -Wall -Wextra -O2 -I modules/06-hft-patterns/lab/measure modules/05-memory-perf/lab/work/pool_tail.cpp -o "${TMPDIR:-/tmp}/pool_tail" && "${TMPDIR:-/tmp}/pool_tail"
printf '%s\n' modules/05-memory-perf/lab/work/pool_tail.cpp | modules/05-memory-perf/checks/pool_tail.check.sh
```

It runs the same 2,000,000-order burst three ways (general heap, lazy pool,
prepared pool), each in a fresh child process, and prints a latency histogram
(`lat_hist.hpp`, shared with module 06) plus two counters per run. Fill the
`TODO(M5-LPA2)` in `prepare_for_peak`. The check grades only the counters
(zero heap calls while timed in the prepared run, and `prepare_for_peak`
working for other peaks), never a time. The starter prints `RESULT: TODO`.
