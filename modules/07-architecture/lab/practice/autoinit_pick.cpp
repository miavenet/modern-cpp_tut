// autoinit_pick.cpp - stack vs heap uninitialized read, for testing what -ftrivial-auto-var-init covers.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O0 -ftrivial-auto-var-init=pattern modules/07-architecture/lab/practice/autoinit_pick.cpp -o "${TMPDIR:-/tmp}/autoinit_pick" && "${TMPDIR:-/tmp}/autoinit_pick"
// Stack vs heap uninitialized read, used to test what -ftrivial-auto-var-init covers.
#include <cstdio>
#include <cstdlib>
__attribute__((noinline)) int pick_stack(const int* v, int n) {
  int best;                       // BUG: never set when n == 0
  for (int i = 0; i < n; ++i) if (i == 0 || v[i] > best) best = v[i];
  return best;
}
__attribute__((noinline)) int pick_heap(const int* v, int n) {
  int* best = static_cast<int*>(std::malloc(sizeof(int)));   // BUG: same, but heap
  for (int i = 0; i < n; ++i) if (i == 0 || v[i] > *best) *best = v[i];
  int r = *best; std::free(best); return r;
}
int main(int argc, char**) {
  int v[3] = {4, 9, 2};
  int n = argc - 1;               // run with no arguments -> n == 0
  int s = pick_stack(v, n);
  std::printf("stack value 0x%x -> %s\n", (unsigned)s, s > 5 ? "big" : "small");
  int h = pick_heap(v, n);
  std::printf("heap  -> %s\n", h > 5 ? "big" : "small");
}
