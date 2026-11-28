// line_report.cpp - a hardware-report line for "cache line size", with fixtures and checks.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/line_report.cpp -o "${TMPDIR:-/tmp}/line_report" && "${TMPDIR:-/tmp}/line_report"
// Exit 0: every check passes. Exit 1: a check failed. Exit 2: no negative tests written yet.
// Your task: write the negative tests in negative_tests(), watch them fail, then fix render().
#include <cstdio>
#include <optional>
#include <string>

// What the probes returned. std::nullopt means the probe gave no value.
struct Inputs {
  std::optional<long> host_os_line;    // the host OS report (sysctl hw.cachelinesize)
  std::optional<long> guest_os_line;   // the guest OS report (getconf LEVEL1_DCACHE_LINESIZE in a VM)
  std::optional<long> destructive;     // std::hardware_destructive_interference_size, if the macro is defined
  std::optional<long> constructive;    // std::hardware_constructive_interference_size, if the macro is defined
};

// Fixtures: three machines that are NOT the course Mac.
const Inputs F1{64, 128, 128, 64};                         // all four values present
const Inputs F2{std::nullopt, 64, std::nullopt, std::nullopt};  // no host report; library lacks the macro
const Inputs F3{std::nullopt, std::nullopt, 256, 64};      // no OS report at all; recommendation only

// Report format, one line per field, each ending in "\n":
//   host_os_line=<n>  guest_os_line=<n>  recommended_destructive=<n>  recommended_constructive=<n>
// The draft below is what a colleague wrote. Fix it once your negative tests catch it.
std::string render(const Inputs& in) {
  auto num = [](std::optional<long> v) { return std::to_string(v.value_or(0)); };
  std::string r;
  r += "observed_cache_line=" + num(in.destructive) + "\n";
  r += "host_os_line=" + num(in.host_os_line) + "\n";
  r += "guest_os_line=" + num(in.guest_os_line) + "\n";
  r += "recommended_destructive=" + num(in.destructive) + "\n";
  r += "recommended_constructive=" + num(in.constructive) + "\n";
  return r;
}

// ---- checks ----
int g_fail = 0;
bool has_line(const std::string& report, const std::string& line) {
  return ("\n" + report).find("\n" + line + "\n") != std::string::npos;   // a whole line, not a prefix
}
void check(const char* what, bool ok) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_fail;
}

void positive_tests() {
  const std::string r1 = render(F1), r2 = render(F2), r3 = render(F3);
  check("F1 host_os_line=64", has_line(r1, "host_os_line=64"));
  check("F1 guest_os_line=128", has_line(r1, "guest_os_line=128"));
  check("F1 recommended_destructive=128", has_line(r1, "recommended_destructive=128"));
  check("F1 recommended_constructive=64", has_line(r1, "recommended_constructive=64"));
  check("F2 guest_os_line=64", has_line(r2, "guest_os_line=64"));
  check("F3 recommended_destructive=256", has_line(r3, "recommended_destructive=256"));
}

// Write your negative tests here with check(...). Return how many checks you wrote.
int negative_tests() {
  return 0;
}

int main() {
  positive_tests();
  const int negatives = negative_tests();
  if (negatives == 0) {
    std::puts("TODO negative tests: none written yet");
    return 2;
  }
  std::printf("%d negative checks, %d failures\n", negatives, g_fail);
  return g_fail == 0 ? 0 : 1;
}
