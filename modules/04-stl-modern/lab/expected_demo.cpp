// expected_demo.cpp - std::expected (C++23): a value or an error, no throw needed.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++23 -Wall -Wextra modules/04-stl-modern/lab/expected_demo.cpp -o "${TMPDIR:-/tmp}/expected_demo" && "${TMPDIR:-/tmp}/expected_demo"
#include <charconv>
#include <cstdio>
#include <expected>
#include <string_view>

enum class ParseError { Empty, NotANumber };

std::expected<int, ParseError> parseQty(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseError::Empty);
    int q = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), q);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return std::unexpected(ParseError::NotANumber);
    return q;                                    // success
}

int main() {
    for (std::string_view in : {"100", "", "12x"}) {
        auto r = parseQty(in);
        if (r) std::printf("'%.*s' -> %d\n", (int)in.size(), in.data(), *r);
        else   std::printf("'%.*s' -> error %d\n", (int)in.size(), in.data(), (int)r.error());
    }
    parseQty("12x");   // result ignored: no warning from Apple clang 21 -Wall -Wextra
    try { (void)parseQty("").value(); }
    catch (const std::bad_expected_access<ParseError>&) {
        std::puts("value() on an error threw bad_expected_access");
    }
}
