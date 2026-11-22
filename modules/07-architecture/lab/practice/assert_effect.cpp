// assert_effect.cpp - a counter bumped inside assert() versus before it (second build adds -DNDEBUG).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/assert_effect.cpp -o "${TMPDIR:-/tmp}/assert_effect" && "${TMPDIR:-/tmp}/assert_effect"
//   clang++ -std=c++20 -O2 -Wall -Wextra -DNDEBUG modules/07-architecture/lab/practice/assert_effect.cpp -o "${TMPDIR:-/tmp}/assert_effect_release" && "${TMPDIR:-/tmp}/assert_effect_release"
#include <cassert>
#include <cstdio>
int commits = 0;
bool commit() { ++commits; return true; }
int main() {
    assert(commit());
    std::printf("embedded=%d\n", commits);
    commits = 0;
    const bool ok = commit();
    assert(ok);
    (void)ok;
    std::printf("separate=%d\n", commits);
}
