// clamp.cpp - clamp negative values to zero: two element widths, two ways to write the store.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   bash modules/05-memory-perf/lab/practice/clamp_sve.sh clang++   (NEON, then +sve; counts per function)
// Clamp negative values to zero: two element widths, two ways to write it.
// No headers on purpose, so the file also cross-compiles with no sysroot.
using size_type = decltype(sizeof 0);

void clamp32_if(int* a, size_type n) {
    for (size_type i = 0; i < n; ++i)
        if (a[i] < 0) a[i] = 0;            // store only when needed
}
void clamp8_if(signed char* a, size_type n) {
    for (size_type i = 0; i < n; ++i)
        if (a[i] < 0) a[i] = 0;            // store only when needed
}
void clamp32_always(int* a, size_type n) {
    for (size_type i = 0; i < n; ++i)
        a[i] = a[i] < 0 ? 0 : a[i];        // store every element
}
void clamp8_always(signed char* a, size_type n) {
    for (size_type i = 0; i < n; ++i)
        a[i] = a[i] < 0 ? static_cast<signed char>(0) : a[i];  // store every element
}
