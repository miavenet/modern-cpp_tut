// reader_lock.cpp - what a shared (reader) lock compiles to in your standard library.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/03-concurrency/lab/practice/reader_lock.cpp -o -   (read the calls in read_value)
// What does a shared (reader) lock compile to in your standard library?
#include <shared_mutex>
std::shared_mutex g_m;
int g_value;
int read_value() {
    std::shared_lock<std::shared_mutex> lk(g_m);
    return g_value;
}
