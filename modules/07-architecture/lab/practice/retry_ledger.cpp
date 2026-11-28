// retry_ledger.cpp - a retried-write ledger, checked with the attempt and completion identities.
// Two separate exercises, chosen by a required mode argument:
//   complete  six logical writes with {1,3,1,2,4,2} attempts; every final attempt succeeds
//   drop      five logical writes with {2,1,3,1,2} attempts; the last attempt of the
//             three-attempt write fails permanently
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/retry_ledger.cpp -o "${TMPDIR:-/tmp}/retry_ledger" && "${TMPDIR:-/tmp}/retry_ledger" complete
#include <cstdio>
#include <string_view>
#include <vector>
int main(int argc,char** argv) {
    const std::string_view mode=argc>1?argv[1]:"";
    if(mode!="complete" && mode!="drop") {
        std::fprintf(stderr,"usage: retry_ledger complete|drop\n");
        return 2;
    }
    const bool drop=mode=="drop";
    const std::vector<int> tries=drop?std::vector<int>{2,1,3,1,2}:std::vector<int>{1,3,1,2,4,2};
    int attempts=0, completed=0, failed=0;
    for(int required:tries) {
        for(int k=1;k<=required;++k) {
            ++attempts;
            if(k<required || (drop && required==3)) ++failed;
            else ++completed;
        }
    }
    const bool partition=attempts==completed+failed;
    const bool completion=completed==static_cast<int>(tries.size());
    std::printf("mode=%s logical=%zu attempts=%d completed=%d failed=%d\n",drop?"drop":"complete",tries.size(),attempts,completed,failed);
    std::printf("attempt_partition=%d logical_completion=%d\n",partition,completion);
    // Exit 0 when the ledger shows this mode's intended pattern.
    return (partition && completion!=drop)?0:1;
}
