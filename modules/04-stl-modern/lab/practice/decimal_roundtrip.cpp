// decimal_roundtrip.cpp - to_chars precision 6 versus shortest form for 1.0 and its next double; checks from_chars round trips.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/decimal_roundtrip.cpp -o "${TMPDIR:-/tmp}/decimal_roundtrip" && "${TMPDIR:-/tmp}/decimal_roundtrip"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <system_error>
int main() {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53);
    const std::array<double,2> values{1.0,std::nextafter(1.0,2.0)};
    for (bool short_form : {false,true}) {
        std::array<std::string,2> text;
        unsigned matched=0;
        for (std::size_t i=0;i<values.size();++i) {
            char out[64];
            auto r=short_form ? std::to_chars(out,out+64,values[i])
                : std::to_chars(out,out+64,values[i],std::chars_format::general,6);
            if(r.ec!=std::errc{}) return 1;
            text[i].assign(out,r.ptr);
            double back=0;
            auto p=std::from_chars(text[i].data(),text[i].data()+text[i].size(),back);
            if(p.ec!=std::errc{} || p.ptr!=text[i].data()+text[i].size()) return 2;
            matched+=back==values[i];
        }
        std::printf("%s distinct_texts=%d exact_roundtrips=%u\n",short_form?"shortest":"precision6",text[0]==text[1]?1:2,matched);
    }
}
