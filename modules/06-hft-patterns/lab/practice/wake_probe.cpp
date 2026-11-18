// wake_probe.cpp - periodic wakeup probe: absolute deadlines vs relative sleeps.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/wake_probe.cpp -o "${TMPDIR:-/tmp}/wake_probe" && "${TMPDIR:-/tmp}/wake_probe"
// Periodic wakeup probe: absolute deadlines vs relative sleeps.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
using clk = std::chrono::steady_clock;
using std::chrono::microseconds;
using std::chrono::duration_cast;

static void run(bool absolute, int n, microseconds period) {
    std::vector<long> late_us;
    late_us.reserve(n);
    const auto start = clk::now();
    auto t = start;
    for (int k = 1; k <= n; ++k) {
        auto deadline = absolute ? start + k * period : t + period;
        if (absolute) std::this_thread::sleep_until(deadline);
        else          std::this_thread::sleep_for(period);
        t = clk::now();
        late_us.push_back(duration_cast<microseconds>(t - deadline).count());
    }
    long drift = duration_cast<microseconds>(t - (start + n * period)).count();
    std::sort(late_us.begin(), late_us.end());
    std::printf("%-8s n=%d  min_late=%ldus  p50=%ldus  max=%ldus  end_drift=%ldus  negative_lateness=%ld\n",
                absolute ? "absolute" : "relative", n, late_us.front(), late_us[n / 2],
                late_us.back(), drift,
                (long)std::count_if(late_us.begin(), late_us.end(), [](long v) { return v < 0; }));
}

int main() {
    run(true, 500, microseconds(1000));
    run(false, 500, microseconds(1000));
}
