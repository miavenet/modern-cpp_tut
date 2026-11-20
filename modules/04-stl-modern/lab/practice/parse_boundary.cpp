// parse_boundary.cpp - from_chars into a uint32_t (initially 99) for six inputs, then a direct and a transactional parse helper.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary"
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <string_view>
bool direct(std::string_view s,std::uint32_t& out){
    auto r=std::from_chars(s.data(),s.data()+s.size(),out);
    return r.ec==std::errc{} && r.ptr==s.data()+s.size();
}
bool transactional(std::string_view s,std::uint32_t& out){
    std::uint32_t tmp=0;
    if(!direct(s,tmp))return false;
    out=tmp;return true;
}
int main(){
    for(std::string_view s:{"37","37ms"," 37","+37","4294967296",""}){
        std::uint32_t out=99;
        auto r=std::from_chars(s.data(),s.data()+s.size(),out);
        const char* e=r.ec==std::errc{}?"ok":r.ec==std::errc::invalid_argument?"invalid":"range";
        std::printf("[%.*s] error=%s consumed=%td value=%u complete=%d\n",static_cast<int>(s.size()),s.data(),e,r.ptr-s.data(),out,r.ec==std::errc{}&&r.ptr==s.data()+s.size());
    }
    std::uint32_t a=77,b=77;
    bool da=direct("37ms",a), db=transactional("37ms",b);
    std::printf("direct accepted=%d out=%u; transactional accepted=%d out=%u\n",da,a,db,b);
}
