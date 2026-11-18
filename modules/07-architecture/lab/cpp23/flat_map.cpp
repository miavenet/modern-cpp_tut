// flat_map.cpp -- std::flat_map (C++23) stores keys and values in two vectors, ordered by key.
// clang++ -std=c++23 modules/07-architecture/lab/cpp23/flat_map.cpp -o "${TMPDIR:-/tmp}/flat" && "${TMPDIR:-/tmp}/flat"
#include <flat_map>
#include <cstdio>
#include <string>

int main() {
    std::flat_map<std::string, double> px;   // ticker -> last price
    px["MSFT"] = 410.5;
    px["AAPL"] = 227.1;
    px["IBM"]  = 210.0;
    // keys() and values() expose the two underlying containers.
    for (const auto& k : px.keys()) std::printf("%s ", k.c_str());
    std::printf("\nvalues: ");
    for (double v : px.values()) std::printf("%.1f ", v);
    std::printf("\nlookup IBM -> %.1f\n", px.at("IBM"));
}
