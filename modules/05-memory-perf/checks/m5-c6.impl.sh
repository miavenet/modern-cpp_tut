#!/bin/bash
# Check for M5-C6: the O(1) free-list core of a fixed-size block pool
# (compile-verified). Learner's answer on stdin: the three fill-ins, in order.
# Blanks:  A) next   B) free_   C) free_
#   void* pop()      { node* n = free_; free_ = n->__A__; return n; }
#   void  push(void* p){ node* n=(node*)p; n->next = __B__; __C__ = n; }
answer=$(cat)

IFS=, read -r A B C extra <<< "$answer"
A=$(printf '%s' "$A" | xargs)
B=$(printf '%s' "$B" | xargs)
C=$(printf '%s' "$C" | xargs)
if [ "$A" != next ] || [ "$B" != free_ ] || [ "$C" != free_ ] || [ -n "$extra" ]; then
    echo 'MISS: give the three free-list blanks in order'
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing'; exit 2; fi
tmp=$(mktemp -d) || {
    echo 'SETUP PROBLEM: cannot create a temporary build directory'
    exit 2
}
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >/dev/null 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'
    exit 2
fi
cat > "$tmp/t.cpp" <<CPP
#include <cstddef>
struct pool {
    struct node { node* next; };
    node* free_ = nullptr;
    void* pop() {                 // O(1) allocate: pop the free-list head
        node* n = free_;
        free_ = n->$A;            // blank A
        return n;
    }
    void push(void* p) {          // O(1) deallocate: push onto the head
        node* n = static_cast<node*>(p);
        n->next = $B;             // blank B
        $C = n;                   // blank C
    }
};
int main() {
    pool pl;
    pool::node a{}, b{};
    pl.push(&a);
    pl.push(&b);
    if (pl.pop() != &b) return 2;
    if (pl.pop() != &a) return 3;
    if (pl.free_ != nullptr) return 4;
    return 0;
}
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    if "$tmp/a.out"; then
        echo "HIT (free-list pop/push compiles and round-trips O(1))"; exit 0
    else
        echo "MISS: compiles but push/pop does not round-trip (wrong head handling)"; exit 1
    fi
else
    echo "MISS: filled-in free-list code does not compile:"
    echo "$err" | head -3
    exit 1
fi
