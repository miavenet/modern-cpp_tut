// rolling_hash.cpp - a rolling string hash in int and in uint32_t; run it under UBSan.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -fsanitize=undefined modules/07-architecture/lab/practice/rolling_hash.cpp -o "${TMPDIR:-/tmp}/rolling_hash" && "${TMPDIR:-/tmp}/rolling_hash" abc && "${TMPDIR:-/tmp}/rolling_hash" abcdefghijkl
// A rolling hash, the kind that ends up inside a benchmark loop.
#include <cstdint>
#include <cstdio>
#include <cstring>
int hash_signed(const char* s) {            // int arithmetic: can overflow
  int h = 0;
  for (; *s; ++s) h = h * 31 + (unsigned char)*s;
  return h;
}
uint32_t hash_unsigned(const char* s) {     // unsigned arithmetic: wraps mod 2^32, defined
  uint32_t h = 0;
  for (; *s; ++s) h = h * 31u + (unsigned char)*s;
  return h;
}
int main(int argc, char** argv) {
  const char* s = argc > 1 ? argv[1] : "abc";
  std::printf("len=%zu signed=%d unsigned=%u\n", std::strlen(s), hash_signed(s), hash_unsigned(s));
}
