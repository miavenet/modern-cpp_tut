// hex_buffer.cpp - to_chars of 0xbeef into four bytes of a six-byte buffer; bounded view vs C-string read.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/hex_buffer.cpp -o "${TMPDIR:-/tmp}/hex_buffer" && "${TMPDIR:-/tmp}/hex_buffer"
#include <array>
#include <charconv>
#include <cstdio>
#include <string_view>
#include <system_error>
int main() {
    std::array<char, 6> out{'?', '?', '?', '?', '!', '\0'};
    auto r = std::to_chars(out.data(), out.data()+4, 0xbeefu, 16);
    if (r.ec != std::errc{}) return 1;
    const std::string_view field(out.data(), static_cast<std::size_t>(r.ptr-out.data()));
    std::printf("length=%zu ptr_at_end=%d next=%c\n", field.size(), r.ptr==out.data()+4, out[4]);
    std::printf("bounded=%.*s c_string=%s\n", static_cast<int>(field.size()), field.data(), out.data());
    return !(field == "beef" && out[4]=='!' && out[5]=='\0');
}
