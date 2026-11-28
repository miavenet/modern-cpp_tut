# Lesson 06 — Practice Bank, part 12 of 12 (optional): What This OS Really Offers

**Time: about 28 minutes (estimate)** for 3 cards (8 + 10 + 10 minutes). Each card stands alone: stop after any card and the driver resumes there.

Theme: thread placement on macOS, `clock_gettime` on Linux, and a cache-partitioning compatibility report.

These cards are optional extra practice on measurement, research and C++ techniques, and on how results expire.
Each answer ends with a **Holds for / re-check** line: where the result was true, and how to check it again.
Hands-on cards give their commands; run them from the tutorial root. Build output goes to your temporary folder.
Results come from one machine (Apple M1 Max, macOS 27, Apple clang 21, libc++) unless a card names another toolchain; counts are steadier than timings, which vary with load.

## Cards

### Card M6-PB14 · Recall
Q: A harness pins its reader and writer to chosen cores so that "same cluster" and "different cluster" can be compared. You port it to macOS on Apple silicon and replace the Linux call with the Mach affinity API. A 10-line probe calls `thread_policy_set(..., THREAD_AFFINITY_POLICY, ...)` and prints the return code. What does it print, and what does that do to the comparison?
   ```sh
   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/affinity_probe.cpp -o "${TMPDIR:-/tmp}/affinity_probe" && "${TMPDIR:-/tmp}/affinity_probe"
   ```
   Answer format: the return code, what the API promises even on success, then how to report the results.
<details><summary>Answer</summary>
Key:

  - It returns 46, `KERN_NOT_SUPPORTED` ("(os/kern) service not supported"), here on macOS 27.
  - Even where it returned success, `THREAD_AFFINITY_POLICY` takes affinity-set tags, not CPU ids, and Apple documents these as placement hints, not thread-to-processor binding (Thread Affinity API release notes). So it is not a replacement for a Linux CPU-id pin, and success would not establish pinning.
  - So on this Mac you cannot choose core placement; any "same vs different cluster" result is uncontrolled. Report it as unpinned, or run that part on Linux. If a future OS returns success, treat that as a prompt to check actual placement, not as proof of pinning.
  - The same probe on Linux (`pthread_setaffinity_np` to CPU 0) returns 0 in a container. But in a VM that selects a guest CPU: physical-cluster claims still need the host topology and placement control (card M5-PB12 in module 05).
  - Holds for / re-check: macOS 27 on M1 Max, Apple clang 21 SDK; Linux container on the same host. Apple could change behaviour later; re-run the probe on each OS release. The contract statement rests on Apple's documentation (developer.apple.com, Thread Affinity API release notes: "explicit thread to processor binding is not supported"; the notes predate Apple silicon, so they give the contract, not current support); the return code on the actual run.
</details>

### Card M6-PB28 · Apply
Q: (Linux only.) A wiki says "reading the clock is free on Linux: `clock_gettime` never enters the kernel." `clock_ids.cpp` makes 1000 `clock_gettime` calls with one clock id chosen by index on the command line: 0 REALTIME, 1 MONOTONIC, 2 MONOTONIC_RAW, 3 BOOTTIME, 4 PROCESS_CPUTIME_ID, 5 THREAD_CPUTIME_ID. `clock_vs_getppid.cpp` also makes 1000 `getppid()` calls. Count the system calls for each (the vDSO is a small library the kernel maps into every process so some calls can run without entering the kernel). Which claims hold, and what else could break them?
   ```sh
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_ids.cpp -o "${TMPDIR:-/tmp}/clock_ids" && for id in 0 1 2 3 4 5; do strace -c -e trace=clock_gettime "${TMPDIR:-/tmp}/clock_ids" $id; done
   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/clock_vs_getppid.cpp -o "${TMPDIR:-/tmp}/clock_vs_getppid" && strace -c "${TMPDIR:-/tmp}/clock_vs_getppid"
   ```
   Answer format: syscalls per clock id and for `getppid`, then what the claim depends on and how to re-check.
<details><summary>Answer</summary>
Key:

  - REALTIME, MONOTONIC, MONOTONIC_RAW, BOOTTIME: 0 syscalls (served by the vDSO). PROCESS_CPUTIME_ID and THREAD_CPUTIME_ID: 1000 syscalls each. `getppid()`: 1000 syscalls.
  - So the claim depends on the clock id. It also depends on the kernel and the clock source: the vdso(7) man page records kernels and architectures where some clocks fall back to the real system call. This container's clock source was `arch_sys_counter`.
  - Re-check on the target host: run the same strace count, and read `/sys/devices/system/clocksource/clocksource0/current_clocksource`.
  - Holds for / re-check: Linux only: kernel 7.0.12-linuxkit, aarch64, glibc (Ubuntu 24.04 container). Results can differ on other kernels, architectures, clock sources and under some virtualization; re-measure there.
</details>

### Card M6-PB31 · Recall
Q: Write the layers of a read-only compatibility report for "can this Linux box reserve part of the L3 for one thread?" (cache partitioning: Intel calls it Cache Allocation Technology, AMD L3 Cache Allocation Enforcement; *resctrl* is the Linux filesystem that exposes it). Keep four things apart: CPU enumeration, kernel support, mount exposure and permissions. Give the CPUID order for Intel, including the asymmetric case. Then run `rdt_probe.sh` (it counts RDT flags in `/proc/cpuinfo`, checks `/proc/filesystems` for resctrl, and lists `/sys/fs/resctrl`) in an arm64 Linux container and say what it can and cannot show.
   ```sh
   bash modules/06-hft-patterns/lab/practice/rdt_probe.sh
   ```
   Answer format: Intel CPUID order, AMD check, kernel checks, the container result, then what the probe cannot show.
<details><summary>Answer</summary>
Key:

  - CPU, Intel (check leaf and sub-leaf availability first; query per logical processor): asymmetric first. If CPUID.(7,1):ECX[1] is set, leaf 28H is the selected leaf and may differ between logical processors. Otherwise, if CPUID.(7,0):EBX[15] (`RDT_A`) is set, leaf 10H is selected. In the selected leaf's resource bitmap, test the L3 bit (leaf 10H sub-leaf 0 EBX[1] `CAT_L3`; leaf 28H has the same definitions). Asymmetric leaves can exist without informative symmetric ones, and symmetric platforms can enumerate asymmetric leaves, so neither set alone decides.
  - CPU, AMD: CPUID Fn0000_0007 EBX bit 15 (`PQE`), then leaf 10H for the L3 resource; the class count is MAX_COS+1 and the mask length CBM_LEN+1.
  - Kernel: flags such as `rdt_a`, `cat_l3`, `cdp_l3`, `mba` in `/proc/cpuinfo`; `resctrl` listed in `/proc/filesystems`; mount it (`mount -t resctrl resctrl /sys/fs/resctrl`) and read `info/L3/cbm_mask`, `num_closids` and `sparse_masks` for the real limits.
  - Container result: 0 RDT flags, no resctrl filesystem, no `/sys/fs/resctrl`. This container exposes no listed RDT flags or resctrl interface; the probe alone cannot distinguish absent hardware, unavailable kernel support/modules, or hidden/unmounted/inaccessible interfaces. It is an exposure probe, not proof of CPU incapability, and a mounted resctrl is not proof of an exclusive reservation. It shows nothing about any x86 server.
  - Report each layer separately, and run it in the environment you deploy on: a VM or container may hide what the host CPU has.
  - Holds for / re-check: the x86 parts (Intel CPUID, AMD, kernel resctrl) are citations from the Intel SDM, the AMD APM and kernel.org, not results: nothing was run on x86. The only run is the arm64 Linux container (Ubuntu 24.04, linuxkit kernel 7.0.12), as an exposure result. The asymmetric enumeration shows the CPUID recipe itself changes over time. Arm has its own partitioning architecture (not covered by these sources, not checked). Re-check: rerun `rdt_probe.sh` on the target and reread the current SDM CPUID tables.
</details>

---

## Close

That is the whole bank for this module. Missed cards come back tomorrow. After a toolchain or machine change, re-run a card's check before trusting its numbers.
