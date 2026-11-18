// scalar_fp.cpp - a loop that must stay scalar (total) and one that vectorizes (twice).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   bash modules/07-architecture/lab/practice/scalar_fp.sh clang++
// A loop that must stay scalar: each step needs the previous result,
// and floating-point addition may not be reordered without -ffast-math.
using size_type = decltype(sizeof 0);
double total(const double* x, size_type n) {
    double t = 0.0;
    for (size_type i = 0; i < n; ++i) t += x[i];
    return t;
}
// For contrast: element-wise work with no chain between iterations.
void twice(double* x, size_type n) {
    for (size_type i = 0; i < n; ++i) x[i] *= 2.0;
}
