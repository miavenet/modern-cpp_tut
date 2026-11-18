#!/bin/bash
# Check for M4-C2: std::optional completion (compile-verified + run)
# Learner's answer on stdin. Blanks: 1) optional 2) nullopt 3) idx.
answer=$(cat)

IFS=, read -r b1 b2 b3 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*optional[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*nullopt[[:space:]]*$ &&
   "$b3" =~ ^[[:space:]]*idx[[:space:]]*$ ]] || {
    echo "MISS: answer exactly optional, nullopt, idx in that order"
    exit 1
}
b1=optional; b2=nullopt; b3=idx

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
#include <optional>
#include <vector>
#include <iostream>
std::$b1<std::size_t> findIndex(const std::vector<int>& v, int target) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == target) return i;
    return std::$b2;
}
int main() {
    std::vector<int> v = {1, 42};
    if (auto idx = findIndex(v, 42))
        std::cout << *$b3 << '\n';
    if (findIndex(v, 99)) return 3;
    if (auto idx = findIndex(v, 1)) std::cout << *idx << '\n';
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    out=$("$tmp/a.out")
    if [ "$out" = $'1\n0' ]; then
        echo "Printed index: $out"
        echo "HIT (found indices and missing target checked)"; exit 0
    else
        printf 'SETUP PROBLEM: known-good specimen expected indices <1, 0> and no value for a missing target; got <%s>\n' "$out"
        exit 2
    fi
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
