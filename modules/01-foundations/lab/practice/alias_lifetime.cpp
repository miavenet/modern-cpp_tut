// alias_lifetime.cpp - two aliasing shared_ptr<int> views of one Tile; pointer vs owner identity and when the Tile is destroyed.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/01-foundations/lab/practice/alias_lifetime.cpp -o "${TMPDIR:-/tmp}/alias_lifetime" && "${TMPDIR:-/tmp}/alias_lifetime"
#include <cstdio>
#include <memory>
struct Tile {
    int left = 12;
    int right = 34;
    static inline int destroyed = 0;
    ~Tile() { ++destroyed; }
};
int main() {
    auto owner = std::make_shared<Tile>();
    std::shared_ptr<int> left(owner, &owner->left);
    std::shared_ptr<int> right(owner, &owner->right);
    const bool same_owner = !left.owner_before(right) && !right.owner_before(left);
    std::printf("same_pointer=%d same_owner=%d owners=%ld\n",
                left.get() == right.get(), same_owner, owner.use_count());
    owner.reset();
    std::printf("after_owner_reset destroyed=%d owners=%ld values=%d,%d\n",
                Tile::destroyed, left.use_count(), *left, *right);
    left.reset();
    std::printf("after_left_reset destroyed=%d owners=%ld value=%d\n",
                Tile::destroyed, right.use_count(), *right);
    right.reset();
    std::printf("after_right_reset destroyed=%d\n", Tile::destroyed);
    return Tile::destroyed != 1;
}
