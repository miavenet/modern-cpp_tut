#!/bin/bash
# Check for M6-C1: SPSC release/acquire memory orderings (compile-verified)
# Learner's answer on stdin. Blanks: 1) release (producer store) 2) acquire
# (consumer load). seq_cst/relaxed compile but are caught by the card RejectIf.
answer=$(cat)

b1=$(echo "$answer" | grep -oE 'release' | head -1)
b2=$(echo "$answer" | grep -oE 'acquire' | head -1)

[ -z "$b1" ] && { echo "MISS: blank 1 (memory_order_release on the store) not identified"; exit 1; }
[ -z "$b2" ] && { echo "MISS: blank 2 (memory_order_acquire on the load) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <atomic>
#include <cstddef>
struct Ring {
    std::atomic<size_t> head_{0};
    int slots_[256];
    static constexpr size_t kMask = 255;
    void push(size_t h, int v) {
        slots_[h & kMask] = v;
        head_.store(h + 1, std::memory_order_$b1);
    }
    size_t pop() {
        auto h = head_.load(std::memory_order_$b2);
        return h;
    }
};
int main() { Ring r; r.push(0, 5); (void)r.pop(); }
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
