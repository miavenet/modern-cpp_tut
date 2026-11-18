// spans.cpp - one workload, two latency definitions (argument: burst size).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/spans.cpp -o "${TMPDIR:-/tmp}/spans" && "${TMPDIR:-/tmp}/spans" 64 && "${TMPDIR:-/tmp}/spans" 1
// One workload, two latency definitions.
//   service  = handler start -> handler end
//   response = message arrival -> handler end
// A burst of messages arrives first; then one worker handles them in order.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using clk = std::chrono::steady_clock;

static std::uint64_t handle(std::uint64_t x) {          // some fixed work per message
    for (int i = 0; i < 2000; ++i) x = x * 6364136223846793005ULL + 1442695040888963407ULL;
    return x;
}

int main(int argc, char** argv) {
    const int burst = argc > 1 ? std::atoi(argv[1]) : 64;
    const int rounds = 200;
    std::vector<double> service, response;
    std::uint64_t sink = 0;
    long nested_ok = 0, total = 0, last_ge_sum = 0;
    std::vector<clk::time_point> arrival(static_cast<std::size_t>(burst));
    for (int r = 0; r < rounds; ++r) {
        for (int i = 0; i < burst; ++i) arrival[static_cast<std::size_t>(i)] = clk::now();   // burst arrives
        double sum_service = 0, last_response = 0;
        for (int i = 0; i < burst; ++i) {
            auto t0 = clk::now();
            sink += handle(static_cast<std::uint64_t>(r * burst + i));
            auto t1 = clk::now();
            double s = std::chrono::duration<double, std::nano>(t1 - t0).count();
            double q = std::chrono::duration<double, std::nano>(t1 - arrival[static_cast<std::size_t>(i)]).count();
            service.push_back(s); response.push_back(q);
            ++total; if (q >= s) ++nested_ok;
            sum_service += s; last_response = q;
        }
        if (last_response >= sum_service) ++last_ge_sum;
    }
    std::sort(service.begin(), service.end()); std::sort(response.begin(), response.end());
    double s50 = service[service.size() / 2], r50 = response[response.size() / 2];
    std::printf("burst=%d messages=%ld\n", burst, total);
    std::printf("response >= service for %ld of %ld messages\n", nested_ok, total);
    std::printf("last message of a burst: response >= sum of all service times in %ld of %d bursts\n", last_ge_sum, rounds);
    std::printf("median response is %s median service (ratio %.1f)\n", r50 > s50 ? "LARGER than" : "not larger than", r50 / s50);
    std::printf("(sink %llu)\n", static_cast<unsigned long long>(sink));
    return 0;
}
