// relocate_record.cpp - memcpy a record whose pointer selects one of its own bytes, then the same with an integer offset.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/relocate_record.cpp -o "${TMPDIR:-/tmp}/relocate_record" && "${TMPDIR:-/tmp}/relocate_record"
// Both records stay alive, so no dangling pointer is dereferenced.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
struct AddressRecord{char bytes[4];const char* selected;};
struct OffsetRecord{char bytes[4];std::uint32_t selected;};
int main(){
    AddressRecord a{{'A','B','C','D'},nullptr};a.selected=&a.bytes[1];
    AddressRecord b{};std::memcpy(&b,&a,sizeof a);
    a.bytes[1]='Z';
    std::printf("pointer_record trivial=%d layout=%d points_to_source=%d local=%c selected=%c\n",std::is_trivially_copyable_v<AddressRecord>,std::is_standard_layout_v<AddressRecord>,b.selected==&a.bytes[1],b.bytes[1],*b.selected);
    OffsetRecord c{{'A','B','C','D'},1},d{};
    std::memcpy(&d,&c,sizeof c);c.bytes[1]='Z';
    if(d.selected>=sizeof d.bytes)return 1;
    std::printf("offset_record selected=%c source=%c\n",d.bytes[d.selected],c.bytes[c.selected]);
}
