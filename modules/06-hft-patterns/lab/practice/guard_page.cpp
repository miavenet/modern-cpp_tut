// guard_page.cpp - put an inaccessible guard region right after a buffer, so a write past the end crashes at once.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/guard_page.cpp -o "${TMPDIR:-/tmp}/guard_page" && "${TMPDIR:-/tmp}/guard_page"
// Map buffer + guard as one anonymous region, then mprotect(PROT_NONE) the
// guard. Try buffer sizes 4096 and sysconf(_SC_PAGESIZE). For each size that
// works, a child process writes one byte past the end; the parent reports
// how the child ended.
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static void attempt(size_t bufsz, size_t page) {
  size_t total = bufsz + page;
  char* base = (char*)mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (base == MAP_FAILED) { std::perror("mmap"); return; }
  int rc = mprotect(base + bufsz, page, PROT_NONE);
  std::printf("buffer %zu B: mprotect(guard at base+%zu) -> %s\n", bufsz, bufsz, rc == 0 ? "ok" : std::strerror(errno));
  if (rc == 0) {
    base[bufsz - 1] = 1;  // last byte of the buffer: fine
    pid_t pid = fork();
    if (pid == 0) { ((volatile char*)base)[bufsz] = 1; _exit(0); }  // one byte past the end
    int st = 0;
    waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) std::printf("  child writing base[%zu]: killed by signal %d (%s)\n", bufsz, WTERMSIG(st), strsignal(WTERMSIG(st)));
    else std::printf("  child writing base[%zu]: exited normally (overrun NOT caught)\n", bufsz);
  }
  munmap(base, total);
}

int main() {
  size_t page = (size_t)sysconf(_SC_PAGESIZE);
  std::printf("page size: %zu\n", page);
  attempt(4096, page);
  attempt(page, page);
}
