# SPSC ring lab and runnable samples

The edit exercise is in `spsc/starter/spsc_ring.hpp`; its two `TODO(M6-C1)`
lines are the edit sites. The reference is in `spsc/solution/`.
From the tutorial root, check the edited starter with:

```sh
printf '%s\n' modules/06-hft-patterns/lab/spsc/starter | modules/06-hft-patterns/checks/spsc_lab.check.sh
```

The SPSC lesson gives the direct compiler command and expected test result.
The other folders contain runnable examples with commands in their lessons.

The MPMC example also checks capacity two: it prints
`capacity-two full/empty: PASS` before its concurrent test. Capacity one must
fail at compile time with `N must be a power of two and at least 2`:

```sh
clang++ -std=c++20 -Wall -Wextra -fsyntax-only \
  modules/06-hft-patterns/lab/mpmc/test_capacity_one.cpp
```
