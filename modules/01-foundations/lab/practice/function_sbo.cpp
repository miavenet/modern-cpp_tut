// function_sbo.cpp - count heap allocations std::function makes for captures of growing size, and for copy / move of a large target.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/function_sbo.cpp -o "${TMPDIR:-/tmp}/function_sbo" && "${TMPDIR:-/tmp}/function_sbo"
// Count heap allocations made by std::function for captures of growing size,
// and for copy / move of a large target.
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <array>
#include <type_traits>

static long g_allocs = 0;
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <std::size_t N>
long allocs_for_capture() {
    std::array<char, N> pad{};
    pad[0] = 1;
    long before = g_allocs;
    std::function<int(int)> f = [pad](int x) { return x + pad[0]; };
    long after = g_allocs;
    volatile int sink = f(1);
    (void)sink;
    return after - before;
}

struct alignas(16) Over { char c[8]; };             // 16-byte aligned, 16 bytes total
// 8-byte captures that differ only in which special member functions are user-provided.
struct ThrowMove {                                  // user-provided move, may throw
    char c[8]{};
    ThrowMove() = default;
    ThrowMove(const ThrowMove&) = default;
    ThrowMove(ThrowMove&&) noexcept(false) {}
};
struct NoexceptMove {                               // user-provided move, noexcept
    char c[8]{};
    NoexceptMove() = default;
    NoexceptMove(const NoexceptMove&) = default;
    NoexceptMove(NoexceptMove&&) noexcept {}
};
struct ThrowCopy {                                  // user-provided throwing copy, noexcept move
    char c[8]{};
    ThrowCopy() = default;
    ThrowCopy(const ThrowCopy&) noexcept(false) {}
    ThrowCopy(ThrowCopy&&) noexcept = default;
};

template <class T>
long allocs_for_type(const char* name) {
    T t; long b0 = g_allocs;
    std::function<int(int)> f = [t](int x) { return x + t.c[0]; };
    std::printf("8-byte capture, %s -> %ld allocations (trivially copyable: %d, nothrow move: %d, nothrow copy: %d)\n",
        name, g_allocs - b0, (int)std::is_trivially_copyable_v<T>,
        (int)std::is_nothrow_move_constructible_v<T>, (int)std::is_nothrow_copy_constructible_v<T>);
    return 0;
}

int main() {
    std::printf("sizeof(std::function<int(int)>) = %zu\n", sizeof(std::function<int(int)>));
    std::printf("capture bytes -> allocations\n");
    std::printf("  8 -> %ld\n", allocs_for_capture<8>());
    std::printf(" 16 -> %ld\n", allocs_for_capture<16>());
    std::printf(" 24 -> %ld\n", allocs_for_capture<24>());
    std::printf(" 32 -> %ld\n", allocs_for_capture<32>());
    std::printf(" 40 -> %ld\n", allocs_for_capture<40>());
    std::printf(" 64 -> %ld\n", allocs_for_capture<64>());
    {   Over o{}; long b0 = g_allocs;
        std::function<int(int)> f = [o](int x) { return x + o.c[0]; };
        std::printf("16-byte capture with alignof 16 -> %ld allocations (alignof=%zu)\n", g_allocs - b0, alignof(Over)); (void)f; }
    allocs_for_type<ThrowMove>("throwing move ctor");
    allocs_for_type<NoexceptMove>("user-provided noexcept move ctor");
    allocs_for_type<ThrowCopy>("throwing copy ctor, noexcept move");
    // Large target: 32-byte capture, stored on the heap.
    std::array<char, 32> big{}; big[0] = 2;
    std::function<int(int)> g = [big](int x) { return x + big[0]; };
    long b = g_allocs;
    std::function<int(int)> g_copy = g;
    long d1 = g_allocs - b; b = g_allocs;
    std::function<int(int)> g_move = std::move(g_copy);
    long d2 = g_allocs - b;
    std::printf("large target: copy %ld, move %ld, result %d\n", d1, d2, g_move(1));
    std::printf("move ctor noexcept: %d\n", (int)std::is_nothrow_move_constructible_v<std::function<int(int)>>);
}
