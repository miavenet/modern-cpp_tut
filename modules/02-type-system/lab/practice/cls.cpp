// cls.cpp - a constexpr byte-class table for a tiny number scanner, checked by static_assert.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/cls.cpp -o "${TMPDIR:-/tmp}/cls" && "${TMPDIR:-/tmp}/cls" "-12.5" "+7x" " 3"
//   clang++ -std=c++20 -O2 -S modules/02-type-system/lab/practice/cls.cpp -o -   (read classify)
#include <array>
#include <cstdio>
// Character classes for a tiny number scanner.
enum Cls : unsigned char { Other, Digit, Sign, Dot, Space };
constexpr std::array<Cls, 256> make_table() {
  std::array<Cls, 256> t{};                 // all Other
  for (int c = '0'; c <= '9'; ++c) t[c] = Digit;
  t['+'] = Sign; t['-'] = Sign; t['.'] = Dot; t[' '] = Space; t['\t'] = Space;
  return t;
}
constexpr auto kCls = make_table();
// Checked by the compiler, not at run time:
static_assert(kCls['7'] == Digit);
static_assert(kCls['-'] == Sign && kCls['x'] == Other);
Cls classify(unsigned char c) { return kCls[c]; }
int main(int argc, char** argv) {
  int counts[5] = {};
  for (int i = 1; i < argc; ++i)
    for (const char* p = argv[i]; *p; ++p) ++counts[classify((unsigned char)*p)];
  std::printf("other %d digit %d sign %d dot %d space %d\n",
              counts[0], counts[1], counts[2], counts[3], counts[4]);
}
