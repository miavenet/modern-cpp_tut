#!/bin/bash
# Check for M4-C5: zero-copy FIX-tag parsing (compile-verified)
# Learner's answer on stdin. Both blanks are string_view (param type and the
# pointer+length constructor).
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*string_view[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*string_view[[:space:]]*$ ]] || {
    echo "MISS: answer exactly string_view, string_view in that order"
    exit 1
}
b1=string_view
b2=string_view

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
#include <string>
#include <string_view>
void parseTag(std::$b1 tagData) { (void)tagData; }
int main() {
    std::string fixMsg = "35=D|49=CLIENT|";
    parseTag(std::$b1(fixMsg.data(), 4)); // pass "35=D"
}
CPP

if err=$(clang++ -std=c++20 -Wall -Wextra -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "SETUP PROBLEM: known-good specimen does not compile:"
    echo "$err" | head -3
    exit 2
fi
