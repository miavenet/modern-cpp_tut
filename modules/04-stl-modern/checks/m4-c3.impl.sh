#!/bin/bash
# Check for M4-C3: std::visit on a variant (compile-verified)
# Learner's answer on stdin. Blanks: 1) visit 2) v (the variant object).
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*visit[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*v[[:space:]]*$ ]] || {
    echo "MISS: answer exactly visit, v in that order"
    exit 1
}
b1=visit
b2=v

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
#include <variant>
#include <iostream>
int main() {
    std::variant<int, float> v = 42;
    std::$b1([](auto&& val) {
        std::cout << val;
    }, $b2);
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
