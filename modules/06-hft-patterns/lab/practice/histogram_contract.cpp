// histogram_contract.cpp - two separate exercises, chosen by a required mode argument:
//   quantile  what half-open bucket counts preserve (exact p90 of two datasets)
//   merge     adding count arrays by position, and a merge guard that refuses unequal edges
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/histogram_contract.cpp -o "${TMPDIR:-/tmp}/histogram_contract" && "${TMPDIR:-/tmp}/histogram_contract" quantile
#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>
struct Histogram {
    std::array<int,4> edges;
    std::array<unsigned,3> counts{};
    void add(int x) {
        for (unsigned i=0;i<counts.size();++i)
            if(x>=edges[i] && x<edges[i+1]) { ++counts[i]; return; }
        throw std::out_of_range("sample");
    }
};
std::optional<Histogram> merge(const Histogram& a,const Histogram& b) {
    if(a.edges!=b.edges) return std::nullopt;
    auto result=a;
    for(unsigned i=0;i<a.counts.size();++i) result.counts[i]+=b.counts[i];
    return result;
}
int rank90(std::vector<int> values) {
    std::sort(values.begin(),values.end());
    // Fixture is nonempty and tiny: ceil(9*N/10), converted to zero-based index.
    return values[(9*values.size()+9)/10-1];
}
int main(int argc,char** argv) {
    const std::string_view mode=argc>1?argv[1]:"";
    if(mode=="quantile") {
        Histogram a{{0,10,20,40}}, b{{0,10,20,40}};
        const std::vector<int> av{1,2,3,4,19}, bv{1,2,3,4,10};
        for(int x:av) a.add(x);
        for(int x:bv) b.add(x);
        std::printf("A=%u,%u,%u B=%u,%u,%u equal=%d p90A=%d p90B=%d\n",
            a.counts[0],a.counts[1],a.counts[2],b.counts[0],b.counts[1],b.counts[2],
            a.counts==b.counts,rank90(av),rank90(bv));
        return 0;
    }
    if(mode=="merge") {
        Histogram x{{0,8,16,32}}, y{{0,4,16,32}}, x2{{0,8,16,32}}, pooled{{0,8,16,32}};
        x.add(6);
        for(int v:{6,12}) { y.add(v); x2.add(v); }
        for(int v:{6,6,12}) pooled.add(v);
        const auto bad=merge(x,y), valid=merge(x,x2);
        std::printf("naive=%u,%u,%u pooled=%u,%u,%u rejected=%d\n",
            x.counts[0]+y.counts[0],x.counts[1]+y.counts[1],x.counts[2]+y.counts[2],
            pooled.counts[0],pooled.counts[1],pooled.counts[2],!bad.has_value());
        if(!valid) { std::printf("matching_edges_accepted=0\n"); return 1; }
        std::printf("matching_edges_accepted=1 counts=%u,%u,%u\n",
            valid->counts[0],valid->counts[1],valid->counts[2]);
        return bad.has_value() || valid->counts!=pooled.counts;
    }
    std::fprintf(stderr,"usage: histogram_contract quantile|merge\n");
    return 2;
}
