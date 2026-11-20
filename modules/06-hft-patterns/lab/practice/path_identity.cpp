// path_identity.cpp - a toy trace collector keyed by the sum of two function IDs versus one keyed by the full ordered pair.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/path_identity.cpp -o "${TMPDIR:-/tmp}/path_identity" && "${TMPDIR:-/tmp}/path_identity"
// The sum key is deliberately poor; it says nothing about any real profiler's collision rate.
#include <array>
#include <map>
#include <cstdio>
int main(){
    using Path=std::array<unsigned,2>;
    const std::array<Path,5> events{{{2,5},{2,5},{5,2},{2,5},{5,2}}};
    std::map<unsigned,unsigned> hash_only;
    std::map<Path,unsigned> exact;
    for(const auto& p:events){++hash_only[p[0]+p[1]];++exact[p];}
    std::printf("hash_records=%zu full_path_records=%zu\n",hash_only.size(),exact.size());
    std::printf("merged=%u path_2_5=%u path_5_2=%u\n",hash_only.at(7),exact.at(Path{2,5}),exact.at(Path{5,2}));
}
