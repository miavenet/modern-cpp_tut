// expected.cpp -- Returns validation errors as values. Calling .value() on an error throws std::bad_expected_access.
// clang++ -std=c++23 modules/07-architecture/lab/cpp23/expected.cpp -o "${TMPDIR:-/tmp}/expected" && "${TMPDIR:-/tmp}/expected"
#include <cstdio>
#include <cmath>
#include <expected>

enum class Err { BadPrice };

std::expected<long, Err> to_ticks(double px) {
    if (!std::isfinite(px) || px <= 0 || px > 1'000'000.0)
        return std::unexpected(Err::BadPrice);
    return static_cast<long>(px * 100);
}

int main() {
    auto ok = to_ticks(101.25);
    auto bad = to_ticks(-1);
    std::printf("ok: %ld   bad has a value: %s\n", ok.value(), bad.has_value() ? "yes" : "no");
}
