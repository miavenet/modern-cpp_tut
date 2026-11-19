// gcd_fold.cpp - one recursive gcd asked for four ways: which ones are folded to 21?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/02-type-system/lab/practice/gcd_fold.cpp -o -   (then g++ the same way)
// Same recursion, three ways of asking for the answer.
static unsigned gcd(unsigned a, unsigned b) { return b == 0 ? a : gcd(b, a % b); }
constexpr unsigned gcd_ce(unsigned a, unsigned b) { return b == 0 ? a : gcd_ce(b, a % b); }

unsigned ask_optimizer()  { return gcd(1071u, 462u); }       // hope it folds
unsigned ask_constexpr_fn() { return gcd_ce(1071u, 462u); }  // allowed to fold, not required
unsigned ask_forced() { constexpr unsigned r = gcd_ce(1071u, 462u); return r; } // must fold
unsigned ask_runtime(unsigned a, unsigned b) { return gcd(a, b); }
