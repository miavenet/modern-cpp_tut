// alias_truth.cpp - bool and use_count of an alias with no owner and of an alias that owns but exposes nullptr.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/alias_truth.cpp -o "${TMPDIR:-/tmp}/alias_truth" && "${TMPDIR:-/tmp}/alias_truth"
#include <cstdio>
#include <memory>
int main() {
    int local = 23; // Outlives every alias; never transferred to an owner.
    std::shared_ptr<int> empty;
    std::shared_ptr<int> borrowed(empty, &local);
    auto owner = std::make_shared<int>(41);
    std::shared_ptr<int> keeper(owner, nullptr);
    owner.reset();
    std::printf("borrowed bool=%d owners=%ld value=%d\n",
                static_cast<bool>(borrowed), borrowed.use_count(), *borrowed);
    std::printf("keeper bool=%d owners=%ld\n",
                static_cast<bool>(keeper), keeper.use_count());
    return !(borrowed && borrowed.use_count() == 0 && !keeper && keeper.use_count() == 1);
}
