// stream_store.cpp - a plain fill and a fill through clang's non-temporal store builtin, runtime length.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/stream_store.cpp -o -   (then g++ the same way, and x86-64 clang on Compiler Explorer)
// A portable way to ask for a streaming (non-temporal) store.
void fill_normal(int* dst, int v, int n) {
    for (int i = 0; i < n; ++i) dst[i] = v;
}
void fill_stream(int* dst, int v, int n) {
    for (int i = 0; i < n; ++i) __builtin_nontemporal_store(v, &dst[i]);
}
