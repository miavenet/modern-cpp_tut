// lookup_materialization.cpp - counts Key temporaries built by unordered_set::find(int) with transparent and opaque (-DOPAQUE) hash and equality.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/04-stl-modern/lab/practice/lookup_materialization.cpp -o "${TMPDIR:-/tmp}/lookup_materialization" && "${TMPDIR:-/tmp}/lookup_materialization"
//   clang++ -std=c++20 -O2 -Wall -Wextra -DOPAQUE modules/04-stl-modern/lab/practice/lookup_materialization.cpp -o "${TMPDIR:-/tmp}/lookup_opaque" && "${TMPDIR:-/tmp}/lookup_opaque"
#include <cstdio>
#include <unordered_set>
struct Key {
    int value;
    static inline int constructed=0;
    Key(int x):value(x) { ++constructed; }
};
struct Hash {
#ifndef OPAQUE
    using is_transparent=void;
#endif
    std::size_t operator()(const Key& k) const { return std::hash<int>{}(k.value); }
    std::size_t operator()(int k) const { return std::hash<int>{}(k); }
};
struct Equal {
#ifndef OPAQUE
    using is_transparent=void;
#endif
    bool operator()(const Key& a,const Key& b) const { return a.value==b.value; }
    bool operator()(const Key& a,int b) const { return a.value==b; }
    bool operator()(int a,const Key& b) const { return a==b.value; }
};
int main() {
    std::unordered_set<Key,Hash,Equal> keys;
    keys.emplace(4); keys.emplace(12);
    Key::constructed=0;
    int found=0;
    for (int query : {4,8,12}) found += keys.find(query)!=keys.end();
    std::printf("found=%d lookup_key_constructions=%d\n",found,Key::constructed);
    return found!=2;
}
