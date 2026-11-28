// parse_boundary.cpp - two separate exercises, chosen by a required mode argument:
//   boundaries     from_chars into a uint32_t (initially 99) for six inputs
//   transactional  a direct and a transactional parse helper on one rejected input
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/parse_boundary.cpp -o "${TMPDIR:-/tmp}/parse_boundary" && "${TMPDIR:-/tmp}/parse_boundary" boundaries
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
int main(int argc,char** argv){
    const std::string_view mode=argc>1?argv[1]:"";
    if(mode=="boundaries"){
        for(std::string_view s:{"37","37ms"," 37","+37","4294967296",""}){
            std::uint32_t out=99;
            auto r=std::from_chars(s.data(),s.data()+s.size(),out);
            const char* e=r.ec==std::errc{}?"ok":r.ec==std::errc::invalid_argument?"invalid":"range";
            std::printf("[%.*s] error=%s consumed=%td value=%u complete=%d\n",static_cast<int>(s.size()),s.data(),e,r.ptr-s.data(),out,r.ec==std::errc{}&&r.ptr==s.data()+s.size());
        }
        return 0;
    }
    if(mode=="transactional"){
        const std::string_view input="64kb";
        std::uint32_t a=250,b=250;
        bool da=direct(input,a), db=transactional(input,b);
        std::printf("direct accepted=%d out=%u; transactional accepted=%d out=%u\n",da,a,db,b);
        return 0;
    }
    std::fprintf(stderr,"usage: parse_boundary boundaries|transactional\n");
    return 2;
}
