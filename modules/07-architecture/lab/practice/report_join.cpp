// report_join.cpp - merged worker rows compared with reference rows by position and by (variant,size) key.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/report_join.cpp -o "${TMPDIR:-/tmp}/report_join" && "${TMPDIR:-/tmp}/report_join"
// Invented integer counts. Exit 0 means this deliberately bad fixture gave the expected one keyed mismatch.
#include <array>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
struct Row{const char* variant;int size;int count;};
int main(){
    const std::array<Row,4> raw{{{"red",4,12},{"blue",4,8},{"red",8,24},{"blue",8,16}}};
    // Each worker emits in completion order; worker_b has one stale cell.
    const std::array<Row,2> worker_a{{{"red",8,24},{"red",4,12}}};
    const std::array<Row,2> worker_b{{{"blue",8,15},{"blue",4,8}}};
    const std::vector<Row> merged{worker_a[0],worker_b[0],worker_a[1],worker_b[1]};
    using Key=std::pair<std::string,int>;
    std::map<Key,int> index;
    for(auto r:raw)if(!index.emplace(Key{r.variant,r.size},r.count).second)return 2;
    int positional=0, keyed=0;
    std::map<Key,int> seen;
    for(std::size_t i=0;i<merged.size();++i){
        auto r=merged[i];Key k{r.variant,r.size};
        if(!seen.emplace(k,r.count).second)return 3;
        positional+=r.count!=raw[i].count;
        auto found=index.find(k);if(found==index.end())return 4;
        if(found->second!=r.count){++keyed;std::printf("mismatch %s size=%d merged=%d raw=%d\n",r.variant,r.size,r.count,found->second);}
    }
    if(seen.size()!=index.size())return 5;
    std::printf("positional_mismatches=%d keyed_mismatches=%d\n",positional,keyed);
    return positional==4&&keyed==1?0:1;
}
