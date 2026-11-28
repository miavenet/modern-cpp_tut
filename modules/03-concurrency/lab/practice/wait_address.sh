#!/bin/bash
# wait_address.sh - build wait_address.cpp as atomic<int> and atomic<char>, trace each run's futex calls with strace, then classify WAIT and WAKE addresses.
# Linux only (futex and strace are Linux tools); needs g++, strace and python3. Run from the tutorial root:
#   bash modules/03-concurrency/lab/practice/wait_address.sh
# Traces and program output go to a scratch folder in your temp directory; the census is printed by wait_census.py.
set -eu
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d "${TMPDIR:-/tmp}/wait_address.XXXXXX")
uname -a
g++ --version | head -1
printf '#include <version>\n' | g++ -std=c++20 -dM -E -x c++ - | grep '__GLIBCXX__ '
g++ -std=c++20 -O2 -Wall -Wextra -pthread "$D/wait_address.cpp" -o "$W/wait_int"
g++ -std=c++20 -O2 -Wall -Wextra -pthread -DSMALL "$D/wait_address.cpp" -o "$W/wait_char"
for kind in int char; do
  timeout 5 strace -f -e trace=futex -o "$W/$kind.strace" "$W/wait_$kind" > "$W/$kind.stdout" 2> "$W/$kind.stderr"
  cat "$W/$kind.stdout" "$W/$kind.stderr"
done
# The installed header's type predicate and wait-address selection:
hdr=$(printf '#include <atomic>\n' | g++ -std=c++20 -x c++ -M - | tr ' ' '\n' | grep 'bits/atomic_wait.h$' || true)
[ -n "$hdr" ] && grep -n -E '__platform_wait_uses_type|_S_wait_addr|_M_ver' "$hdr" || echo "bits/atomic_wait.h not found"
echo "traces in $W"
python3 "$D/wait_census.py" "$W"
