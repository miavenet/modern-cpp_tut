// views_sv.cpp - string_view (C++17) and span (C++20): pointer + length, no ownership.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/views_sv.cpp -o "${TMPDIR:-/tmp}/views_sv" && "${TMPDIR:-/tmp}/views_sv"
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

int sumBytes(std::span<const std::byte> buf) {          // contiguous std::byte elements
    int total = 0;
    for (std::byte b : buf) total += std::to_integer<int>(b);
    return total;
}

int main() {
    std::string msg = "35=D|44=1275|38=100|";
    std::string_view whole = msg;                        // no copy
    std::string_view price = whole.substr(8, 4);         // "1275", still no copy
    std::printf("price view: '%.*s' size=%zu\n", (int)price.size(), price.data(), price.size());

    int px = 0;
    auto [end, ec] = std::from_chars(price.data(), price.data() + price.size(), px);
    std::printf("from_chars -> %d (ok=%d, consumed=%td)\n", px, ec == std::errc{}, end - price.data());

    std::vector<std::byte> rx(4, std::byte{0x01});
    std::byte raw[3] = {std::byte{2}, std::byte{2}, std::byte{2}};
    std::printf("sum(vector)=%d sum(array)=%d\n", sumBytes(rx), sumBytes(raw));
    std::printf("sizeof(string_view)=%zu sizeof(span<const byte>)=%zu\n",
                sizeof(std::string_view), sizeof(std::span<const std::byte>));
}
