// cache_warm.cpp - is "touched during setup" also "cache-warm"?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/cache_warm.cpp -o "${TMPDIR:-/tmp}/cache_warm" && "${TMPDIR:-/tmp}/cache_warm"
// Both buffers are pre-faulted, so there are no page faults in the timed region.
// Each buffer is 1 MiB of 64-byte nodes linked in a random cycle (a dependent
// pointer chase, so the hardware prefetcher cannot hide misses). W is timed right
// after setup walked it. C is timed after 256 MiB of other memory has been streamed through.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <sys/resource.h>
#include <vector>

struct alignas(64) Node { Node* next; };
static long minflt() { rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_minflt; }
static Node* volatile sink;

static void build(std::vector<Node>& v, std::mt19937& g) {
  std::vector<size_t> p(v.size());
  for (size_t i = 0; i < p.size(); ++i) p[i] = i;
  std::shuffle(p.begin(), p.end(), g);
  for (size_t i = 0; i < p.size(); ++i) v[p[i]].next = &v[p[(i + 1) % p.size()]];
}
static Node* chase(Node* n, size_t steps) { while (steps--) n = n->next; return n; }

static double timed(std::vector<Node>& v, long& faults) {
  long f0 = minflt();
  auto t0 = std::chrono::steady_clock::now();
  sink = chase(&v[0], v.size());
  auto t1 = std::chrono::steady_clock::now();
  faults = minflt() - f0;
  return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

int main() {
  const size_t nodes = (1u << 20) / sizeof(Node), big = 256u << 20;
  std::vector<unsigned char> evict(big, 1);  // pre-faulted by the constructor
  std::mt19937 g(1);
  for (int rep = 0; rep < 5; ++rep) {
    std::vector<Node> w(nodes), c(nodes);
    build(c, g);
    build(w, g);
    sink = chase(&w[0], nodes);  // setup walks W last: W is warm
    long fw, fc;
    double tw = timed(w, fw);
    unsigned s = 0;
    for (size_t i = 0; i < big; i += 64) s += evict[i];  // push C (and W) out
    sink = (Node*)(std::uintptr_t)s;
    double tc = timed(c, fc);
    std::printf("rep %d  warm-from-setup %7.1f us (faults %ld)   after-eviction %7.1f us (faults %ld)\n", rep, tw, fw,
                tc, fc);
  }
}
