// heldout_routes.cpp - pick the opcode to specialize from training input, then count its hits on held-out input.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/heldout_routes.cpp -o "${TMPDIR:-/tmp}/heldout_routes" && "${TMPDIR:-/tmp}/heldout_routes"
// Source-level route counts, not instruction counts or a speed ranking.
#include <array>
#include <cstdio>
int hits(const std::array<int,8>& input,int candidate){int n=0; for(int x:input)n+=x==candidate;return n;}
int main(){
    const std::array<int,8> training{2,2,2,2,2,2,2,9};
    const std::array<int,8> heldout{2,9,9,9,9,9,2,9};
    int chosen=hits(training,2)>hits(training,9)?2:9;
    std::printf("chosen=%d training_hits=%d heldout_hits=%d alternative_heldout_hits=%d\n",chosen,hits(training,chosen),hits(heldout,chosen),hits(heldout,9));
    int errors=0;
    for(int x:heldout){int reference=x*3;int specialized=x==chosen?chosen*3:x*3;errors+=reference!=specialized;}
    std::printf("heldout_result_errors=%d\n",errors);
    return errors;
}
