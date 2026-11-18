// prefault.cpp - minor page faults: first touch of fresh memory vs a hot loop over prefaulted memory (Linux: argument nothp).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault"
//   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/prefault.cpp -o "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" && "${TMPDIR:-/tmp}/prefault" nothp   (Linux)
// Count minor page faults: first touch of fresh memory vs a hot loop over prefaulted memory.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
static long minflt() { rusage r{}; getrusage(RUSAGE_SELF, &r); return r.ru_minflt; }
int main(int argc, char** argv) {
#if defined(__linux__)
  // "nothp": ask the kernel not to back this process with transparent huge pages.
  if (argc > 1 && std::strcmp(argv[1], "nothp") == 0) { int rc = prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0); std::printf("prctl(PR_SET_THP_DISABLE)=%d; PR_GET_THP_DISABLE=%d\n", rc, prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0)); }
#else
  (void)argc; (void)argv;
#endif
  const std::size_t bytes = 64u << 20;            // 64 MiB
  const long page = sysconf(_SC_PAGESIZE);
  char* buf = static_cast<char*>(std::malloc(bytes));
  if (!buf) return 1;
  long f0 = minflt();
  for (std::size_t i = 0; i < bytes; i += page) buf[i] = 1;   // first touch ("prefault")
  long f1 = minflt();
  long sum = 0;
  for (int rep = 0; rep < 3; ++rep)                            // "hot loop" on the same memory
    for (std::size_t i = 0; i < bytes; i += page) { buf[i] += 1; sum += buf[i]; }
  long f2 = minflt();
  std::printf("page=%ld bytes=%zu pages=%zu first_touch_faults=%ld hot_loop_faults=%ld sum=%ld\n",
              page, bytes, bytes / page, f1 - f0, f2 - f1, sum);
  std::free(buf);
}
