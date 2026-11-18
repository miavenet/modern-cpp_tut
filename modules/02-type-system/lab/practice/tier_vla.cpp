// tier_vla.cpp - a table entry used as a plain local array bound: is that a proof of compile-time construction?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/tier_vla.cpp -o "${TMPDIR:-/tmp}/tier_vla" && "${TMPDIR:-/tmp}/tier_vla" 5
//   clang++ -std=c++20 -O2 -Wall -Wextra -Werror=vla-cxx-extension -fsyntax-only modules/02-type-system/lab/practice/tier_vla.cpp
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
 std::array<std::uint8_t,256> table{};
 table[0]=argc>1 ? static_cast<std::uint8_t>(std::atoi(argv[1])) : 3;
 int proof[table[0]];
 std::printf("array bytes=%zu\n",sizeof proof);
}
