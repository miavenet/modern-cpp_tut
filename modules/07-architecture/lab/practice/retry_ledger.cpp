// retry_ledger.cpp - six logical writes with {1,3,1,2,4,2} attempts: check the attempt and completion identities.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/07-architecture/lab/practice/retry_ledger.cpp -o "${TMPDIR:-/tmp}/retry_ledger" && "${TMPDIR:-/tmp}/retry_ledger"
// drop=0: every final attempt succeeds. drop=1: the last attempt of the four-attempt write fails too.
#include <array>
#include <cstdio>
int main() {
    constexpr std::array<int,6> tries{1,3,1,2,4,2};
    bool valid=true;
    for(bool drop:{false,true}) {
        int attempts=0, completed=0, failed=0;
        for(int required:tries) {
            for(int k=1;k<=required;++k) {
                ++attempts;
                if(k<required || (drop && required==4)) ++failed;
                else ++completed;
            }
        }
        std::printf("drop=%d logical=%zu attempts=%d completed=%d failed=%d\n",drop,tries.size(),attempts,completed,failed);
        std::printf("attempt_partition=%d logical_completion=%d\n",attempts==completed+failed,completed==static_cast<int>(tries.size()));
        valid=valid && attempts==13 && completed==(drop?5:6) && failed==(drop?8:7);
    }
    return valid?0:1;
}
