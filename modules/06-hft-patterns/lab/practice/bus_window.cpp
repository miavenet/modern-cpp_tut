// bus_window.cpp - sanity arithmetic for a quoted reply time on a slow serial bus.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/bus_window.cpp -o "${TMPDIR:-/tmp}/bus_window" && "${TMPDIR:-/tmp}/bus_window"
// Sanity arithmetic for a quoted reply time on a slow serial bus:
// bits that arrive in the window, time to receive the whole command,
// and the least time a signal needs to cross the cable and come back.
#include <cstdio>

int main() {
    const double c = 299'792'458.0;                 // exact SI value, vacuum
    struct Case { double rate_bps, window_us, cmd_bytes, cable_m; };
    const Case cases[] = {
        {1e6, 1.5, 16.0, 300.0},
        {10e6, 1.5, 16.0, 300.0},
        {1e6, 1.5, 16.0, 100.0},
    };
    for (const Case& k : cases) {
        double bits_in_window = k.rate_bps * k.window_us * 1e-6;
        double cmd_bits = k.cmd_bytes * 8.0;
        double us_to_receive = cmd_bits / k.rate_bps * 1e6;
        double us_round_trip_cable = 2.0 * k.cable_m / c * 1e6;
        std::printf("rate %.0f Mb/s, window %.1f us, command %.0f bytes, cable %.0f m\n"
                    "  bits that arrive in the window: %.1f\n"
                    "  command size: %.0f bits, time to receive it: %.1f us\n"
                    "  least cable round trip (vacuum speed): %.2f us\n",
                    k.rate_bps / 1e6, k.window_us, k.cmd_bytes, k.cable_m,
                    bits_in_window, cmd_bits, us_to_receive, us_round_trip_cable);
    }
    return 0;
}
