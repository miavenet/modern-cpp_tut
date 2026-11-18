// lambda_snapshot.cpp - closure size, by-value capture as a snapshot, and modifying a by-copy capture (-DBUMP, -DBUMP -DMUTABLE).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/lambda_snapshot.cpp -o "${TMPDIR:-/tmp}/lambda_snapshot" && "${TMPDIR:-/tmp}/lambda_snapshot"
//   clang++ -std=c++20 -O2 -Wall -Wextra -DBUMP -fsyntax-only modules/01-foundations/lab/practice/lambda_snapshot.cpp
#include <cstdio>
int main() {
    int step = 3;
    int total = 0;
    auto apply = [step, &total](int x) { total += step * x; };
    std::printf("sizeof(apply)=%zu\n", sizeof(apply));
#ifdef BUMP
#ifdef MUTABLE
    auto bad = [step]() mutable { ++step; };
#else
    auto bad = [step]() { ++step; };
#endif
    bad();
#endif
    apply(4); step = 9; apply(2);
    std::printf("total=%d\n", total);
}
