// coordinated_omission.cpp -- coordinated omission in 40 lines (a simulation: no real clocks).
// Run from the tutorial root:
//   clang++ -std=c++20 -O2 modules/06-hft-patterns/lab/measure/coordinated_omission.cpp -o "${TMPDIR:-/tmp}/co" && "${TMPDIR:-/tmp}/co"
// A server needs 0.5 ms per request, except one 1-second stall at t = 5 s.
#include <algorithm>
#include <cstdio>
#include <vector>

constexpr double kService = 0.5, kStallAt = 5000, kStall = 1000, kEnd = 10000; // ms

// Time at which a request that starts service at `start` finishes.
double finish(double start) {
    if (start <= kStallAt && start + kService > kStallAt) return start + kService + kStall;
    return start + kService;
}

void report(const char* name, std::vector<double> lat) {
    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) { return lat[std::size_t(p / 100 * (lat.size() - 1))]; };
    std::printf("%-12s n=%5zu  p50=%7.1f  p99=%7.1f  p99.9=%7.1f  max=%7.1f ms\n",
                name, lat.size(), pct(50), pct(99), pct(99.9), lat.back());
}

int main() {
    std::vector<double> closed, open;
    // Closed loop: wait for the reply, then wait 0.5 ms before sending again.
    for (double t = 0; t < kEnd;) {
        double done = finish(t);
        closed.push_back(done - t);
        t = done + 1.0 - kService;          // Normally 1 ms between sends; the stall delays the next send.
    }
    // Open loop: a request ARRIVES every 1 ms whether or not the server is busy.
    double server_free = 0;
    for (double arrive = 0; arrive < kEnd; arrive += 1.0) {
        double start = std::max(arrive, server_free);
        server_free = finish(start);
        open.push_back(server_free - arrive);
    }
    report("closed-loop", closed);
    report("open-loop", open);
}
