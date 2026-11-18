// musttail_rules.cpp - which of these does clang accept under [[clang::musttail]]?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/06-hft-patterns/lab/practice/musttail_rules.cpp -o "${TMPDIR:-/tmp}/musttail_rules.o"
// Which of these does clang accept under [[clang::musttail]]?
int two(int a, int b);
int one(int a);
long wide(int a);
int ok(int a, int b)        { [[clang::musttail]] return two(b, a); }      // same signature
int fewer(int a, int b)     { [[clang::musttail]] return one(a + b); }     // fewer parameters
int widened(int a, int b)   { [[clang::musttail]] return (int)wide(a + b); } // result converted
int escapes(int a, int b)   { int local = a; int* p = &local; (void)b;
                              [[clang::musttail]] return two(*p, b); }     // local read before the call
