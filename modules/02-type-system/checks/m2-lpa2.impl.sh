#!/bin/bash
# Check for M2-LPA2: a per-thread counter that is guaranteed to need no run-time
# initialisation. Learner's answer on stdin: one namespace-scope declaration of `seq`,
# a std::uint64_t. Graded by compiling and running it, never by time:
#   - it must compile as C++20 (a duplicated specifier, a wrong name or type fails);
#   - the compiler's own syntax tree must show constinit ON seq (a word in a string or
#     comment, or on another variable, does not count) and the variable must stay writable (constexpr makes ++seq fail to build);
#   - it must be thread_local: two threads each start from 0 and do not see each
#     other's writes.
# Equivalent spellings pass: any specifier order, `static`, `inline`, `= 0`, `{0}`, `(0)`.
set -u
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*$//')
if [ -z "$answer" ]; then echo 'MISS: give one C++ declaration'; exit 1; fi
case "$answer" in
    *'#'*|*'\'*) echo 'MISS: give one plain declaration'; exit 1 ;;
esac
body=${answer%;}
case "$body" in *';'*) echo 'MISS: give one declaration'; exit 1 ;; esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() {}\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1; then
    echo 'SETUP PROBLEM: the C++ compiler cannot build a trivial program'; exit 2
fi

# constinit is bound to the variable itself by the compiler: the AST must hold a VarDecl
# named seq that carries a ConstInitAttr of its own. A word in a string, a comment, a
# lambda's local, or another variable's declaration does not produce one.
seq_is_constinit() {
    clang++ -std=c++20 -fsyntax-only -Xclang -ast-dump=json -Xclang -ast-dump-filter=seq "$1" 2>/dev/null |
    python3 -I -c '
import json, sys
text, pos, dec, found = sys.stdin.read(), 0, json.JSONDecoder(), False
def walk(n):
    global found
    if isinstance(n, dict):
        if n.get("kind") == "VarDecl" and n.get("name") == "seq" and "tls" in n and \
           any(c.get("kind") == "ConstInitAttr" for c in n.get("inner", [])):
            found = True
        for v in n.values(): walk(v)
    elif isinstance(n, list):
        for v in n: walk(v)
while True:
    text = text[pos:].lstrip()
    if not text: break
    obj, pos = dec.raw_decode(text); walk(obj)
sys.exit(0 if found else 1)'
}
cat >"$tmp/probe.cpp" <<CPP
#include <cstdint>
#include <thread>
#include <type_traits>
$body;
static_assert(std::is_same_v<decltype(seq), std::uint64_t>, "seq must be a std::uint64_t");
int main() {
    if (seq != 0) return 3;                    // starts at 0
    ++seq;                                     // writable
    std::uint64_t other = 99;
    std::thread t([&] { other = seq; seq = 7; });
    t.join();
    if (other != 0) return 4;                  // the new thread has its own copy, starting at 0
    return seq == 1 ? 0 : 5;                   // and its write of 7 did not reach this thread
}
CPP
if ! clang++ -std=c++20 -Wall -Werror=duplicate-decl-specifier "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that declaration does not compile as a writable std::uint64_t named seq'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
if ! seq_is_constinit "$tmp/probe.cpp"; then
    echo 'MISS: nothing makes the compiler guarantee constant initialisation of seq itself: put constinit on the declaration of seq'; exit 1
fi
"$tmp/probe" >"$tmp/run.log" 2>&1
case $? in
    0) echo 'HIT (constinit, writable, one copy per thread, starts at 0)'; exit 0 ;;
    4|5) echo 'MISS: the counter is shared between threads; each thread needs its own (thread_local)'; exit 1 ;;
    *) echo 'MISS: the counter does not start at 0'; exit 1 ;;
esac
