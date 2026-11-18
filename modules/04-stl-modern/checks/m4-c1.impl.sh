#!/bin/bash
# Check for M4-C1: ranges filter/transform pipeline (compile-verified)
# Learner's answer on stdin. Blanks: 1) filter 2) transform.
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*filter[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*transform[[:space:]]*$ ]] || {
    echo "MISS: answer exactly filter, transform in that order"
    exit 1
}
b1=filter
b2=transform

tmp=$(mktemp -d) || {
    echo 'SETUP PROBLEM: cannot create temporary build directory'
    exit 2
}
trap 'rm -rf "$tmp"' EXIT
printf 'int main() { return 0; }\n' > "$tmp/probe.cpp"
if ! clang++ -std=c++20 -Wall -Wextra "$tmp/probe.cpp" -o "$tmp/probe" >/dev/null 2>&1 || ! "$tmp/probe"; then
    echo 'SETUP PROBLEM: compiler or runtime probe failed'
    exit 2
fi
cat > "$tmp/t.cpp" <<CPP
#include <ranges>
#include <vector>
int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5};
    auto result = nums
        | std::views::$b1([](int n) { return n % 2 == 0; })
        | std::views::$b2([](int n) { return n * n; });
    (void)result;
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
