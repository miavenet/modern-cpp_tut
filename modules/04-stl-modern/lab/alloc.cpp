// alloc.cpp - count heap allocations made by std::string and std::format.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/04-stl-modern/lab/alloc.cpp -o "${TMPDIR:-/tmp}/alloc" && "${TMPDIR:-/tmp}/alloc"
#include <cstdio>
#include <cstdlib>
#include <format>
#include <new>
#include <string>

static int allocations = 0;
void* operator new(std::size_t n) {
    ++allocations;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

void parseOrder(const std::string& msg) { (void)msg; }

int main() {
    std::printf("SSO capacity of an empty std::string here: %zu\n", std::string{}.capacity());
    int before = allocations;
    parseOrder("8=FIX.4.2|9=65|");                  // 15 chars: fits inline
    std::printf("short literal -> temporary string: %d allocations\n", allocations - before);
    before = allocations;
    parseOrder("8=FIX.4.2|9=65|35=D|49=CLIENT|");   // 30 chars: too long for inline
    std::printf("long literal  -> temporary string: %d allocations\n", allocations - before);
    before = allocations;
    std::string s = std::format("Price: {:.2f}, Qty: {}", 101.256, 300);  // 23 chars
    std::printf("format of 23 chars: %d allocations\n", allocations - before);
}
