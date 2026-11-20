// parse_rules.cpp - from_chars on "+37" into a signed int, and strtol on " 37" (leading space).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_rules.cpp -o "${TMPDIR:-/tmp}/parse_rules" && "${TMPDIR:-/tmp}/parse_rules"
#include <charconv>
#include <cstdlib>
#include <cstdio>
int main() {
    const char plus[]="+37";
    int value=99;
    auto r=std::from_chars(plus,plus+3,value);
    std::printf("signed_plus invalid=%d consumed=%td value=%d\n",r.ec==std::errc::invalid_argument,r.ptr-plus,value);
    const char space[]=" 37";
    char* end=nullptr;
    long parsed=std::strtol(space,&end,10);
    std::printf("strtol_space value=%ld consumed=%td\n",parsed,end-space);
    return r.ec==std::errc::invalid_argument && r.ptr==plus && value==99 && parsed==37 && end==space+3 ? 0 : 1;
}
