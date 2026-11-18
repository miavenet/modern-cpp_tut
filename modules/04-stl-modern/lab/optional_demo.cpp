// optional_demo.cpp - std::optional (C++17): value or nothing, no sentinel.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/optional_demo.cpp -o "${TMPDIR:-/tmp}/optional_demo" && "${TMPDIR:-/tmp}/optional_demo"
#include <cstdio>
#include <map>
#include <optional>
#include <string>

std::optional<double> getPrice(const std::map<std::string, double>& book,
                               const std::string& symbol) {
    auto it = book.find(symbol);
    if (it == book.end()) return std::nullopt;     // "no value"
    return it->second;                             // engaged with a double
}

struct Point { double x, y, z; };

int main() {
    std::map<std::string, double> book{{"AAPL", 189.5}};
    if (auto p = getPrice(book, "AAPL")) std::printf("AAPL %.1f\n", *p);
    auto missing = getPrice(book, "MSFT");
    std::printf("MSFT engaged? %s, value_or: %.1f\n",
                missing.has_value() ? "yes" : "no", missing.value_or(0.0));
    try {
        (void)missing.value();                     // checked access
    } catch (const std::bad_optional_access&) {
        std::puts("value() on empty optional threw bad_optional_access");
    }
    std::printf("sizeof(double)=%zu sizeof(optional<double>)=%zu\n",
                sizeof(double), sizeof(std::optional<double>));
    std::printf("sizeof(Point)=%zu sizeof(optional<Point>)=%zu\n",
                sizeof(Point), sizeof(std::optional<Point>));
}
