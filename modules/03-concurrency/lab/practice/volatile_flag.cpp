// volatile_flag.cpp - a volatile flag used to publish a plain payload; run it under ThreadSanitizer.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O1 -g -fsanitize=thread modules/03-concurrency/lab/practice/volatile_flag.cpp -o "${TMPDIR:-/tmp}/volatile_flag" && "${TMPDIR:-/tmp}/volatile_flag"
// A volatile flag used to publish a plain payload. Run under ThreadSanitizer.
#include <cstdio>
#include <thread>
struct Config { int a; int b; };
Config g_cfg;
volatile bool g_ready = false;
int main() {
    std::thread writer([] { g_cfg.a = 1; g_cfg.b = 2; g_ready = true; });
    std::thread reader([] {
        while (!g_ready) {}
        std::printf("a=%d b=%d\n", g_cfg.a, g_cfg.b);
    });
    writer.join(); reader.join();
}
