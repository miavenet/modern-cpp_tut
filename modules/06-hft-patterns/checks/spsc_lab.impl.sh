#!/bin/bash
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
header_dir=$(cat)
case "$header_dir" in /*) ;; *) header_dir="$root/$header_dir" ;; esac
if [ ! -f "$header_dir/spsc_ring.hpp" ]; then echo "MISS: spsc_ring.hpp not found in $header_dir"; exit 1; fi
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
if grep -qE 'head_\.store\([^;]*memory_order_relaxed' "$header_dir/spsc_ring.hpp"; then
    echo 'MISS: producer head publication still uses relaxed ordering; use release or stronger'
    exit 1
fi
relaxed_head_loads=$(grep -Ec 'head_\.load\([^;]*memory_order_relaxed' "$header_dir/spsc_ring.hpp" || true)
if [ "$relaxed_head_loads" -gt 1 ]; then
    echo 'MISS: consumer head read still uses relaxed ordering; use acquire or stronger'
    exit 1
fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf 'int main() { return 0; }\n' > "$tmp/probe.cpp"
if ! clang++ -std=c++20 -O1 -g -fsanitize=thread "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/probe-build.log" 2>&1 || ! "$tmp/probe" >"$tmp/probe-run.log" 2>&1; then
    echo 'SETUP PROBLEM: compiler or ThreadSanitizer runtime is unavailable'
    exit 2
fi
if ! clang++ -std=c++20 -O1 -g -fsanitize=thread -I "$header_dir" \
        "$root/modules/06-hft-patterns/lab/spsc/test_spsc.cpp" -o "$tmp/spsc" >"$tmp/build.log" 2>&1; then
    echo 'MISS: the SPSC ring does not compile with ThreadSanitizer'; sed -n '1,3p' "$tmp/build.log"; exit 1
fi
{ "$tmp/spsc" >"$tmp/run.log" 2>&1; rc=$?; } 2>"$tmp/shell.log"
if grep -q 'WARNING: ThreadSanitizer: data race' "$tmp/run.log"; then
    echo 'MISS: ThreadSanitizer reported a data race in the ring'; exit 1
fi
if [ "$rc" -ne 0 ] && grep -qiE 'ThreadSanitizer.*(unsupported|unexpected memory mapping|failed to mmap)' "$tmp/run.log"; then
    echo 'SETUP PROBLEM: ThreadSanitizer cannot run on this platform'; exit 2
fi
if [ "$rc" -ne 0 ] || ! grep -q '^RESULT: PASS' "$tmp/run.log"; then
    echo 'MISS: SPSC behavioral test failed'; grep -E '^(FAIL|RESULT:)' "$tmp/run.log" | head -3; exit 1
fi
echo 'HIT (SPSC behavior passed without a ThreadSanitizer race)'
