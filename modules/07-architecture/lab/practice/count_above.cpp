// count_above.cpp - count elements above a threshold; which compilers vectorize it at -O2?
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -
//   g++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/count_above.cpp -o -   (GCC; then -O3)
int count_above(const int* a, int n, int t) {
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (a[i] > t) ++c;
    return c;
}
