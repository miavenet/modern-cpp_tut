// hex_capacity.cpp - to_chars of 0xbeef into ranges of capacity 0, 3, 4 and 5; error, offset, ptr==last and guard bytes.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/hex_capacity.cpp -o "${TMPDIR:-/tmp}/hex_capacity" && "${TMPDIR:-/tmp}/hex_capacity"
#include <array>
#include <charconv>
#include <cstdio>
#include <system_error>
int main() {
    for (std::size_t capacity : {0u, 3u, 4u, 5u}) {
        std::array<char, 7> out{};
        out.fill('#');
        // The offered range is inside the array, including the empty case.
        char* first = out.data()+1;
        char* last = first+capacity;
        auto r = std::to_chars(first, last, 0xbeefu, 16);
        const bool ok = r.ec == std::errc{};
        const bool small = r.ec == std::errc::value_too_large;
        const bool guards = out.front()=='#' && *last=='#';
        std::printf("capacity=%zu status=%s offset=%td ptr_at_end=%d guards=%d\n",
                    capacity, ok ? "ok" : small ? "too_small" : "unexpected",
                    r.ptr-first, r.ptr==last, guards);
        if (!guards || (!ok && !small)) return 1;
        if (ok) {
            std::printf("  text=%.*s\n", static_cast<int>(r.ptr-first), first);
        }
        // Do not inspect or grade bytes inside a failed output range.
        if (ok != (capacity>=4) || r.ptr != (ok ? first+4 : last)) return 2;
    }
}
