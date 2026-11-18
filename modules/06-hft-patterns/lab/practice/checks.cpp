// checks.cpp - five range checks on one order message, written three ways.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/06-hft-patterns/lab/practice/checks.cpp -o "${TMPDIR:-/tmp}/checks.s" && sh modules/06-hft-patterns/lab/practice/count_branches.sh "${TMPDIR:-/tmp}/checks.s"
// Five range checks on one order message, written three ways.
// Question: how many conditional branches does each version have in the binary?
#include <cstdint>

struct Order { std::uint32_t qty, px, side, tif, venue; };

// (1) early return: five `if` statements in the source
bool ok_early(const Order& o) {
    if (o.qty == 0)      return false;
    if (o.px > 500000)   return false;
    if (o.side > 1)      return false;
    if (o.tif > 3)       return false;
    if (o.venue > 12)    return false;
    return true;
}

// (2) short-circuit &&
bool ok_andand(const Order& o) {
    return o.qty != 0 && o.px <= 500000 && o.side <= 1 && o.tif <= 3 && o.venue <= 12;
}

// (3) non-short-circuit &
bool ok_bitand(const Order& o) {
    return (o.qty != 0) & (o.px <= 500000) & (o.side <= 1) & (o.tif <= 3) & (o.venue <= 12);
}
