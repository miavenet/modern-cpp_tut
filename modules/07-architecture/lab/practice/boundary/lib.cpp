// lib.cpp - "library" side: a tiny accessor, compiled in its own translation unit (the full build and count commands are in app.cpp).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -c modules/07-architecture/lab/practice/boundary/lib.cpp -o "${TMPDIR:-/tmp}/lib.o"   (add -flto for the LTO build)
// "Library" side: a tiny accessor, compiled in its own translation unit.
#include <cstdint>
struct Book { std::uint64_t best_bid; std::uint64_t best_ask; };
extern "C" std::uint64_t book_spread(const Book* b) { return b->best_ask - b->best_bid; }
