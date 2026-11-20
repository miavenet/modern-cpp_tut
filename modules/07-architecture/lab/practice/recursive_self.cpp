// recursive_self.cpp - two recursive lambdas with a deduced this: by value (this auto) and by reference (this const auto&).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++23 -O2 -Wall -Wextra modules/07-architecture/lab/practice/recursive_self.cpp -o "${TMPDIR:-/tmp}/recursive_self" && "${TMPDIR:-/tmp}/recursive_self"
// Cargo's copy constructor counts copies; the counter is reset after each closure is built.
#include <cstdio>
struct Cargo {
    inline static int copies=0;
    int unit=2;
    Cargo()=default;
    Cargo(const Cargo& other):unit(other.unit){++copies;}
};
int main(){
    auto value=[cargo=Cargo{}](this auto self,int depth)->int {
        return depth==0?cargo.unit:cargo.unit+self(depth-1);
    };
    auto reference=[cargo=Cargo{}](this const auto& self,int depth)->int {
        return depth==0?cargo.unit:cargo.unit+self(depth-1);
    };
    Cargo::copies=0; int a=value(3);
    std::printf("value result=%d copies=%d\n",a,Cargo::copies);
    Cargo::copies=0; int b=reference(3);
    std::printf("reference result=%d copies=%d\n",b,Cargo::copies);
}
