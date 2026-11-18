// sockopts.cpp - which TCP socket options this platform's header defines.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/sockopts.cpp -o "${TMPDIR:-/tmp}/sockopts" && "${TMPDIR:-/tmp}/sockopts"
// Which TCP socket options does this platform's header define?
#include <cstdio>
#include <netinet/in.h>
#include <netinet/tcp.h>
int main() {
#ifdef TCP_NODELAY
  std::puts("TCP_NODELAY  yes");
#else
  std::puts("TCP_NODELAY  no");
#endif
#ifdef TCP_QUICKACK
  std::puts("TCP_QUICKACK yes");
#else
  std::puts("TCP_QUICKACK no");
#endif
#ifdef TCP_CORK
  std::puts("TCP_CORK     yes");
#else
  std::puts("TCP_CORK     no");
#endif
#ifdef TCP_NOPUSH
  std::puts("TCP_NOPUSH   yes");
#else
  std::puts("TCP_NOPUSH   no");
#endif
}
