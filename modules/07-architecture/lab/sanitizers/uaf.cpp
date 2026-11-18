// uaf.cpp -- use after free: read an order's price after deleting it.
// ASan:   clang++ -std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer modules/07-architecture/lab/sanitizers/uaf.cpp -o "${TMPDIR:-/tmp}/uaf" && "${TMPDIR:-/tmp}/uaf"
#include <cstdio>

struct Order { long id; double price; };

int main() {
    Order* o = new Order{42, 101.25};
    delete o;
    std::printf("price = %f\n", o->price);   // bug: o was freed on the line above
}
