# Lesson 06 — Practice Bank, part 6 of 12 (optional): Linux Sockets

**Time: about 25 minutes (estimate)** for 2 cards (5 + 20 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: which TCP options exist where, and one system call per datagram against a batch.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB16 · Recall
Q: Which of `TCP_NODELAY`, `TCP_QUICKACK`, `TCP_CORK`, `TCP_NOPUSH` exist on Linux and on macOS, what does the Linux man page tcp(7) say about portability of `TCP_QUICKACK` and `TCP_CORK`, and how do you check the names on a new platform in two minutes?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sockopts.cpp -o "${TMPDIR:-/tmp}/sockopts" && "${TMPDIR:-/tmp}/sockopts"
   ```
   Answer format: the names per platform, the man page's words, then the two-minute check.
<details><summary>Answer</summary>
Key:

  - Header check (`sockopts.cpp`, `#ifdef` on each name, `-Wall -Wextra` clean, run on both): Linux glibc defines `TCP_NODELAY`, `TCP_QUICKACK`, `TCP_CORK` and not `TCP_NOPUSH`; macOS defines `TCP_NODELAY` and `TCP_NOPUSH` and not `TCP_QUICKACK` or `TCP_CORK`.
  - The man page marks both `TCP_QUICKACK` and `TCP_CORK` "should not be used in code intended to be portable".
  - Two-minute check: compile a file that `#ifdef`s each option name on the target, then read that platform's man page for the semantics.
  - Holds for / re-check: Ubuntu 24.04 glibc headers and the macOS SDK with Apple clang 21; tcp(7) on man7.org as opened on 2026-11-17. Nothing is claimed about network behaviour. Re-check headers and man page on each new OS or libc.
</details>

### Card M6-PB26 · Apply
Q: (Linux only: run it on Linux or in a Linux container.) Write a program that sends 64 small UDP datagrams over loopback, either one `sendto()` per datagram or one `sendmmsg()` with 64 entries, then drains them with `recvmmsg(..., MSG_DONTWAIT)`, and prints how many send and receive calls it made. Your receive loop must (1) treat `EINTR` and `EAGAIN`/`EWOULDBLOCK` as retryable, any other error as fatal, and (2) give up after an overall deadline, because UDP can lose datagrams and the loop would otherwise hang. Your send loop must handle `sendmmsg` accepting fewer entries than asked. Compare with the reference `udp_batch.cpp`, then count kernel entries with strace:
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/udp_batch.cpp -o "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" batch
   ```
   Answer format: send and receive calls for each mode, the strace counts, then how to grade the send count and the error handling.
<details><summary>Answer</summary>
Key:

  - One-by-one: 64 `sendto` calls. Batched: 1 `sendmmsg` call, accepted in full. Both: 64 datagrams arrive, and 1 `recvmmsg` call drained all 64, with 0 empty receive calls (3 runs each, identical).
  - strace agrees: 64 sendto + 1 recvmmsg, versus 1 sendmmsg + 1 recvmmsg.
  - Technique: count kernel entries with `strace -c` before and after a change. When the batch is accepted in full, the number of send calls falls from the datagram count to 1. If `sendmmsg` accepts only part of the batch, you need more calls (sendmmsg(2): it returns the number of messages sent, and if that is less than requested the caller can retry with the rest), so grade the send-call count against the number of batches actually accepted, not a fixed factor.
  - Error handling: `recvmmsg` returns -1 with `errno` set on error (recvmmsg(2)); a non-blocking receive with nothing queued gives `EAGAIN`/`EWOULDBLOCK` (recv(2)), which is retryable; an overall deadline turns a lost datagram into a reported failure instead of a hang.
  - Holds for / re-check: Linux only (`sendmmsg`/`recvmmsg` are Linux calls; macOS has neither): Ubuntu 24.04 aarch64 container, GCC 13.3, loopback only. A real NIC and full socket buffers can return partial batches, so only the full-acceptance case was measured. Re-check with strace on the target host.
</details>

---

## Close

Next: part 7, `lesson-06-practice-bank-07-branches.md` (about 22 minutes). Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
