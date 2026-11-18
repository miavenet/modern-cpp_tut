// discard.cpp - is the discarded branch of if constexpr really ignored? (-DSKIP_NT leaves out the non-template function)
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/discard.cpp -o "${TMPDIR:-/tmp}/discard" && "${TMPDIR:-/tmp}/discard"
//   clang++ -std=c++20 -O2 -Wall -Wextra -DSKIP_NT modules/02-type-system/lab/practice/discard.cpp -o "${TMPDIR:-/tmp}/discard" && "${TMPDIR:-/tmp}/discard"
// Is the "dead" branch of if constexpr really ignored?
#include <cstdio>
struct Plain { int id; };
template <class T>
int key_t(const T& x) {
    if constexpr (requires { x.key(); }) return x.key();   // discarded for Plain
    else return x.id;
}
#ifndef SKIP_NT
int key_nt(const Plain& x) {
    constexpr bool has_key = false;
    if constexpr (has_key) return x.key();                  // not in a template
    else return x.id;
}
#endif
int main() {
    Plain p{7};
#ifndef SKIP_NT
    std::printf("%d\n", key_nt(p));
#endif
    std::printf("key_t=%d\n", key_t(p));
}
