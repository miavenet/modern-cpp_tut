// pre_encoded.cpp -- a pre-encoded order message: build the bytes once, patch fields per order.
// Run from the tutorial root:
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/hotpath/pre_encoded.cpp -o "${TMPDIR:-/tmp}/pre_encoded" && "${TMPDIR:-/tmp}/pre_encoded"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

// A made-up 24-byte binary "NewOrder" layout (fictional, for illustration):
// [0..1] msg type 'N','O' | [2..9] symbol, space padded | [10..13] qty (uint32)
// [14..21] price in ticks (int64) | [22..23] reserved. Little-endian host assumed.
struct OrderTemplate {
    std::array<unsigned char, 24> bytes{};
    explicit OrderTemplate(std::string_view symbol) {           // cold: done once at startup
        if (symbol.size() > 8)
            throw std::invalid_argument("symbol exceeds 8 bytes");
        bytes[0] = 'N';
        bytes[1] = 'O';
        std::memset(&bytes[2], ' ', 8);
        if (!symbol.empty())
            std::memcpy(&bytes[2], symbol.data(), symbol.size());
    }
    void set(std::uint32_t qty, std::int64_t price_ticks) {    // hot: two fixed-size copies
        std::memcpy(&bytes[10], &qty, sizeof qty);
        std::memcpy(&bytes[14], &price_ticks, sizeof price_ticks);
    }
};

int main() {
    OrderTemplate msg("MSFT");
    msg.set(100, 4105000);
    for (unsigned char b : msg.bytes) std::printf("%02x ", b);
    std::printf("\n");
}
