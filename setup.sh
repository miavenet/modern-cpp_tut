#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

next_steps() {
    printf 'macOS: install Xcode Command Line Tools with xcode-select --install; install CMake with brew install cmake; rerun ./setup.sh.\n'
    printf 'Linux (Debian/Ubuntu): sudo apt update && sudo apt install python3 clang cmake make; rerun ./setup.sh.\n'
}
trap 'next_steps' ERR
if [[ $# -gt 1 || ( $# -eq 1 && ${1:-} != --benchmark ) ]]; then
    printf 'usage: ./setup.sh [--benchmark]\n' >&2
    exit 2
fi
missing=0
for tool in python3 clang++ cmake; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '%s: %s\n' "$tool" "$("$tool" --version | head -1)"
    else
        printf 'SETUP PROBLEM: %s is missing.\n' "$tool"
        missing=1
    fi
done
if (( missing )); then next_steps; exit 1; fi
if ! python3 - <<'PY'
import sys
assert sys.version_info >= (3, 9), "Python 3.9 or later is needed for str.removeprefix and pathlib.Path.is_relative_to in this course."
PY
then
    printf 'SETUP PROBLEM: Python 3.9 or later is needed for str.removeprefix and pathlib.Path.is_relative_to in the course code.\n'
    next_steps
    exit 1
fi
printf 'Tested on macOS arm64 with Apple clang 21; other versions and platforms are untested.\n'
python_line=$(python3 --version)
if [[ $python_line != 'Python 3.14.6' ]]; then
    printf 'WARNING: tested with Python 3.14.6; you have %s; if the driver fails, try Python 3.14.6.\n' "$python_line"
fi
clang_line=$(clang++ --version | head -1)
if [[ $clang_line != *'Apple clang version 21.0.0'* ]]; then
    printf 'WARNING: tested with Apple clang 21.0.0; you have %s; if a lab fails, try Apple clang 21.0.0.\n' "$clang_line"
fi
probe() {
    local label=$1 standard=$2 code=$3
    if ! printf '%s\n' "$code" | clang++ -std="$standard" -x c++ -fsyntax-only - >/dev/null 2>&1; then
        printf 'WARNING: %s unavailable; affected lesson: %s. See its toolchain note.\n' "$label" "$4"
    fi
}
probe '<generator>' c++23 '#include <generator>
int main() { return 0; }' '03/04'
probe 'std::execution::par' c++20 '#include <execution>
auto p = std::execution::par;' '05/04'
probe '<expected>' c++23 '#include <expected>
std::expected<int,int> e(1);' '04/01'
probe '<print>' c++23 '#include <print>
int main() { std::println("ok"); }' '07/03'
probe 'std::osyncstream' c++20 '#include <syncstream>
#include <iostream>
int main() { std::osyncstream(std::cout) << "ok"; }' '04/03'
if ! printf '%s\n' '#include <syncstream>
#include <iostream>
int main() { std::osyncstream(std::cout) << "ok"; }' | clang++ -std=c++20 -fexperimental-library -x c++ -fsyntax-only - >/dev/null 2>&1; then
    printf 'WARNING: std::osyncstream is unavailable even with -fexperimental-library; affected lesson: 04/03.\n'
fi
probe '<flat_map>' c++23 '#include <flat_map>
std::flat_map<int,int> m;' '07/03'
if [[ $(uname -m) != x86_64 ]]; then
    printf 'WARNING: x86 intrinsics unavailable on this CPU; affected lessons: 03/03 and 06/03. See their toolchain notes.\n'
else
    probe '_mm_pause' c++20 '#include <immintrin.h>
int main() { _mm_pause(); }' '03/03'
fi
if [[ ${1:-} != --benchmark ]]; then
    printf 'Required executables found. Compiler/library feature support and build/run checks have not yet been verified. Google Benchmark is optional: ./setup.sh --benchmark.\n'
    exit 0
fi
command -v git >/dev/null 2>&1 || {
    printf 'SETUP PROBLEM: git is required to download Google Benchmark.\n'
    next_steps
    exit 1
}
repo_root=$PWD
source_dir="$repo_root/third_party/google-benchmark-src"
build_dir="$repo_root/third_party/google-benchmark-build"
install_dir="$repo_root/third_party/google-benchmark"
version=v1.9.5
mkdir -p "$repo_root/third_party"
if [[ ! -d "$source_dir/.git" ]]; then
    git clone --depth 1 --branch "$version" https://github.com/google/benchmark.git "$source_dir"
fi
if [[ $(git -C "$source_dir" describe --tags --exact-match) != "$version" ]]; then
    printf 'SETUP PROBLEM: existing Google Benchmark source is not at %s; remove third_party/google-benchmark-src and rerun.\n' "$version"
    next_steps
    exit 1
fi
cmake -S "$source_dir" -B "$build_dir" -DBENCHMARK_ENABLE_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$install_dir"
cmake --build "$build_dir" --parallel
cmake --install "$build_dir"
printf 'Google Benchmark %s installed in third_party/google-benchmark.\n' "$version"
