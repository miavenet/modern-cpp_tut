// widen_product.cpp - square a 64-bit value into unsigned __int128: cast the product, or cast an operand first?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/widen_product.cpp -o "${TMPDIR:-/tmp}/widen_product" && "${TMPDIR:-/tmp}/widen_product"
// unsigned __int128 is a compiler extension (GCC and Clang).
#include <cstdint>
#include <cstdio>
int main(){
    const std::uint64_t x=(std::uint64_t{1}<<40)+3;
    using Wide=unsigned __int128;
    const Wide late=static_cast<Wide>(x*x);
    const Wide early=static_cast<Wide>(x)*x;
    const auto high=[](Wide n){return static_cast<unsigned long long>(n>>64);};
    const auto low=[](Wide n){return static_cast<unsigned long long>(n);};
    std::printf("late high=%llu low=%llu\n",high(late),low(late));
    std::printf("early high=%llu low=%llu\n",high(early),low(early));
    const Wide expected=(Wide{1}<<80)+(Wide{6}<<40)+9;
    std::printf("early_matches=%d late_matches=%d\n",early==expected,late==expected);
}
