// stream_state.cpp - a helper that leaves std::hex set on its caller's stream versus one that restores the flags.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/stream_state.cpp -o "${TMPDIR:-/tmp}/stream_state" && "${TMPDIR:-/tmp}/stream_state"
#include <iomanip>
#include <iostream>
#include <sstream>
void bad_hex(std::ostream& out, unsigned x) { out << std::hex << x; }
void good_hex(std::ostream& out, unsigned x) {
    const auto old=out.flags();
    out << std::hex << x;
    out.flags(old);
}
int main() {
    std::ostringstream bad,good,independent;
    bad << 15 << ' '; bad_hex(bad,15); bad << ' ' << 15;
    good << 15 << ' '; good_hex(good,15); good << ' ' << 15;
    independent << 15;
    std::cout << "bad=" << bad.str() << "\ngood=" << good.str()
              << "\nindependent=" << independent.str() << '\n';
    return bad.str()!="15 f f" || good.str()!="15 f 15" || independent.str()!="15";
}
