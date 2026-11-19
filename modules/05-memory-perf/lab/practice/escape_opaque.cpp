// escape_opaque.cpp - four loops whose accumulator's address does or does not reach an opaque call.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/05-memory-perf/lab/practice/escape_opaque.cpp -o -   (then g++ the same way)
// Where does a local variable have to live in memory?
void observe(const int* p);  // defined in another translation unit (opaque here)

int sum_local(const int* a, int n) {          // acc never leaves the function
    int acc = 0;
    for (int i = 0; i < n; ++i) acc += a[i];
    return acc;
}

int sum_escape_each(const int* a, int n) {    // &acc handed to opaque code every step
    int acc = 0;
    for (int i = 0; i < n; ++i) { acc += a[i]; observe(&acc); }
    return acc;
}

int sum_escape_before(const int* a, int n) {  // &acc handed out once, before the loop
    int acc = 0;
    observe(&acc);
    for (int i = 0; i < n; ++i) acc += a[i];
    return acc;
}

int sum_snapshot(const int* a, int n) {       // a copy escapes, acc itself does not
    int acc = 0;
    for (int i = 0; i < n; ++i) { acc += a[i]; int snap = acc; observe(&snap); }
    return acc;
}
