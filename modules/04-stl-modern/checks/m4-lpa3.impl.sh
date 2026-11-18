#!/bin/bash
# Check for M4-LPA3: report the failure WITHOUT the exception machinery. Learner's
# answer on stdin: one C++ statement for the blank in
#   std::expected<int, reject> check_qty(int qty) noexcept { if (qty <= 0) ___; return qty; }
# Graded by behaviour and by structure, never by time: the function is compiled with
# -fno-exceptions (any throw fails to build), and must return reject::bad_qty for
# qty <= 0 and the quantity otherwise.
set -u
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*$//')
# One statement; a single braced `{ return ...; }` is the same statement. Strip the
# optional closing `;` and the optional braces, then no other `;` may remain.
answer=${answer%;}
case "$answer" in
    '{'*'}') answer=${answer#\{}; answer=${answer%\}}; answer=$(printf '%s' "$answer" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//; s/;$//; s/[[:space:]]*$//') ;;
esac
if [ -z "$answer" ]; then echo 'MISS: give one C++ statement for the blank'; exit 1; fi
case "$answer" in
    *';'*|*'#'*|*'\'*) echo 'MISS: give one statement, without extra statements'; exit 1 ;;
    throw*|*[[:space:]]throw[[:space:]]*) echo 'MISS: a throw is exactly what this path must not do (it does not even build with -fno-exceptions)'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf '#include <expected>\nint main() { std::expected<int, int> e = 1; return *e - 1; }\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++23 -fno-exceptions "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: this compiler cannot build std::expected (C++23) with -fno-exceptions'; exit 2
fi
cat >"$tmp/probe.cpp" <<CPP
#include <expected>
enum class reject { bad_px, bad_qty };   // bad_qty is NOT 0: a default-constructed error must not pass
std::expected<int, reject> check_qty(int qty) noexcept { if (qty <= 0) $answer; return qty; }
int main() {
    for (int q : {0, -1, -1000}) {
        auto r = check_qty(q);
        if (r.has_value() || r.error() != reject::bad_qty) return 3;
    }
    for (int q : {1, 7, 100000}) {
        auto r = check_qty(q);
        if (!r.has_value() || *r != q) return 4;
    }
    return 0;
}
CPP
if ! clang++ -std=c++23 -fno-exceptions -Wall -Wextra "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that statement does not compile in the blank (with -fno-exceptions)'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
"$tmp/probe"
case $? in
    0) echo 'HIT (bad quantities return reject::bad_qty, good ones pass through, and no exception path exists)'; exit 0 ;;
    3) echo 'MISS: for qty <= 0 the result must hold the error reject::bad_qty, not a value'; exit 1 ;;
    *) echo 'MISS: good quantities no longer come back unchanged'; exit 1 ;;
esac
