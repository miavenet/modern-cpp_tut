// function_vec.cpp - what allocates when a std::function with a 32-byte capture goes into, or grows, a std::vector.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/function_vec.cpp -o "${TMPDIR:-/tmp}/function_vec" && "${TMPDIR:-/tmp}/function_vec"
// What allocates when std::function (32-byte capture, heap-stored) goes into / grows a std::vector?
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <vector>
static long g_allocs = 0;
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() {
    std::array<char, 32> big{}; big[0] = 1;
    std::function<int(int)> h = [big](int x) { return x + big[0]; };
    std::vector<std::function<int(int)>> v;
    v.reserve(8);                                   // spare capacity: no vector-buffer allocation below
    long b = g_allocs;
    v.push_back(h);                                 // copy into reserved space
    std::printf("(1) push_back(copy) into reserved vector: %ld\n", g_allocs - b); b = g_allocs;
    std::function<int(int)> h2 = h; b = g_allocs;   // make a second handler (its own allocation not counted)
    v.push_back(std::move(h2));                     // move into reserved space
    std::printf("(2) push_back(move) into reserved vector: %ld\n", g_allocs - b);
    for (int i = 0; i < 2; ++i) v.emplace_back(h);  // 4 elements in total
    long before = g_allocs;
    v.reserve(64);                                  // reallocation of 4 heap-stored handlers
    std::printf("(3) reserve(64) reallocating 4 elements: %ld (1 = new buffer only, 5 = buffer + 4 copies)\n", g_allocs - before);
    return v[3](1) == 2 ? 0 : 1;
}
