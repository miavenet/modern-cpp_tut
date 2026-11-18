// branch_hint.cpp -- where does the compiler put the rare branch?
// No main(): compile to assembly and compare. From the tutorial root:
//   clang++ -std=c++20 -O2 -S -o - modules/06-hft-patterns/lab/measure/branch_hint.cpp | grep -A10 '^__Z6handleib:'
//   clang++ -std=c++20 -O2 -DHINT -S -o - modules/06-hft-patterns/lab/measure/branch_hint.cpp | grep -A10 '^__Z6handleib:'
// (Symbol names above are as printed on macOS; other platforms are untested here.)
void on_halt();         // rare, slow path (defined elsewhere)
int  price_fast(int);   // common path

int handle(int px, bool halted) {
#ifdef HINT
    if (halted) [[unlikely]] { on_halt(); return 0; }
#else
    if (halted) { on_halt(); return 0; }
#endif
    return price_fast(px);
}
