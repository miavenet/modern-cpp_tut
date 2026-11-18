// assume_mask.cpp - a false [[assume]] is undefined behaviour. No sanitizer; compare -O levels.
// Build and run from the tutorial root; the program goes to your temp directory:
//   for o in 0 1 2; do clang++ -std=c++23 -O$o modules/05-memory-perf/lab/practice/assume_mask.cpp -o "${TMPDIR:-/tmp}/assume$o" && "${TMPDIR:-/tmp}/assume$o" 3 && "${TMPDIR:-/tmp}/assume$o" 9; done
#include <cstdio>
#include <cstdlib>

int table[8] = {1, 2, 3, 4, 5, 6, 7, 8};

__attribute__((noinline)) int lookup(int i) {
  [[assume(i >= 0 && i < 8)]];
  return table[i & 7];
}

int main(int argc, char** argv) {
  int i = argc > 1 ? std::atoi(argv[1]) : 3;
  std::printf("lookup(%d)=%d\n", i, lookup(i));
}
