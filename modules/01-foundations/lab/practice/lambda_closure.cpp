// lambda_closure.cpp - a lambda with one by-value and one by-reference capture; paste it into C++ Insights (cppinsights.io, C++20) to see the class the compiler generates.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/lambda_closure.cpp -o "${TMPDIR:-/tmp}/lambda_closure" && "${TMPDIR:-/tmp}/lambda_closure"
#include <cstdio>
int main() {
    int step = 3;
    int total = 0;
    auto apply = [step, &total](int x) { total += step * x; };
    apply(4);
    std::printf("total=%d\n", total);
}
