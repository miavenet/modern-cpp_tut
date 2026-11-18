// packed_ref.cpp - taking the address of, and binding a reference to, a packed member.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -c modules/05-memory-perf/lab/practice/packed_ref.cpp -o "${TMPDIR:-/tmp}/packed_ref.o"
//   g++ -std=c++20 -Wall -Wextra -c modules/05-memory-perf/lab/practice/packed_ref.cpp -o "${TMPDIR:-/tmp}/packed_ref.o"   (GCC)
#include <cstdint>
struct __attribute__((packed)) Pk { uint8_t tag; uint32_t qty; uint16_t venue; };
void bump(uint32_t& q) { ++q; }
uint32_t* addr(Pk& p) { return &p.qty; }
void use(Pk& p) { bump(p.qty); }
