#!/bin/bash
# Check for M2-LPA4 (constinit lab). Learner's answer on stdin: the path to their
# constinit_guard.cpp. Grades structure and counters, never a time:
#   1. the compiler's syntax tree shows constinit on a thread_local variable named seq
#      (a word in a comment or string, or on another variable, does not count);
#   2. a probe that includes the file calls next_id() from two threads, one after the
#      other: each must count 1, 2, 3 on its own (per-thread counter) and the run-time
#      function start_value() must never have been called (g_init_calls == 0);
#   3. the optimised machine code of the file contains no __cxa_guard_acquire.
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
source_path=$(head -n 1 | sed "s/^[[:space:]]*//; s/[[:space:]]*\$//; s/^[\"'\`]//; s/[\"'\`]\$//; s/^[[:space:]]*//; s/[[:space:]]*\$//")
case "$source_path" in /*) ;; *) source_path="$root/$source_path" ;; esac
if [ ! -f "$source_path" ]; then echo "MISS: workspace file not found: $source_path"; exit 1; fi
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
if ! seq_is_constinit "$source_path"; then
    echo 'MISS: no thread_local variable named seq carries constinit: it is what makes the compiler refuse a run-time initialiser'; exit 1
fi
if ! clang++ -std=c++20 -Wall -Wextra -O2 "$source_path" -o "$tmp/lab" >"$tmp/build.log" 2>&1; then
    echo 'MISS: constinit_guard.cpp does not compile'; grep -m3 'error' "$tmp/build.log"; exit 1
fi
cat >"$tmp/probe.cpp" <<CPP
#define main lab_main
#include "$source_path"
#undef main
int main() {
    for (int t = 0; t < 3; ++t) {
        std::uint64_t a = 0, b = 0, c = 0;
        std::thread([&] { a = next_id(); b = next_id(); c = next_id(); }).join();
        if (a != 1 || b != 2 || c != 3) return 3;      // not a fresh per-thread counter
    }
    return g_init_calls == 0 ? 0 : 4;                  // a run-time initialiser ran
}
CPP
if ! clang++ -std=c++20 -O1 "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/probe.log" 2>&1; then
    echo 'MISS: keep the names g_init_calls, start_value and next_id: the check could not build'; grep -m2 'error' "$tmp/probe.log"; exit 1
fi
"$tmp/probe" >/dev/null 2>&1
case $? in
    0) ;;
    3) echo 'MISS: each new thread must start counting at 1: the counter must be thread_local, not shared'; exit 1 ;;
    4) echo 'MISS: start_value() still ran at run time: the counter must start from the constant 0'; exit 1 ;;
    *) echo 'MISS: the probe program failed'; exit 1 ;;
esac
guards=$(clang++ -std=c++20 -O2 -S -o - "$source_path" 2>/dev/null | grep -c __cxa_guard_acquire)
if [ "${guards:-0}" -ne 0 ]; then
    echo "MISS: the machine code still calls __cxa_guard_acquire ($guards time(s)): a function-local static is still there"; exit 1
fi
echo 'HIT (per-thread counters, no run-time initialiser, no initialisation guard in the machine code)'
exit 0
