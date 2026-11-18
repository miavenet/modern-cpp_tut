#!/bin/bash
# Check for M7-C2: modern target-based CMake completion (REAL cmake configure+build).
# Learner's answer on stdin. Blanks we grade:
#   1) target_compile_features (requests cxx_std_20 on the target + consumers)
#   2) target_link_libraries   (links + inherits usage requirements)
#   3) a scope keyword: PRIVATE / PUBLIC / INTERFACE
# We extract the learner's tokens, substitute them into an embedded 2-target
# CMakeLists + tiny sources, and run CMake. A missing token, or a
# legacy/global answer (link_directories / bare include_directories) that never
# yields these target_* tokens, short-circuits to MISS *before* cmake runs, so
# empty stdin returns instantly. On the happy path this configures AND builds a
# real library and executable.
answer=$(cat)

tcf=$(echo "$answer"   | grep -oE 'target_compile_features' | head -1)
tll=$(echo "$answer"   | grep -oE 'target_link_libraries'   | head -1)
scope=$(echo "$answer" | grep -oE 'PRIVATE|PUBLIC|INTERFACE' | head -1)

[ -z "$tcf" ]   && { echo "MISS: blank 1 (target_compile_features for cxx_std_20) not identified"; exit 1; }
[ -z "$tll" ]   && { echo "MISS: blank 2 (target_link_libraries) not identified"; exit 1; }
[ -z "$scope" ] && { echo "MISS: blank 3 (a PRIVATE/PUBLIC/INTERFACE scope keyword) not identified"; exit 1; }

command -v cmake >/dev/null 2>&1 || { echo "SETUP PROBLEM: cmake not found on PATH"; exit 2; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/probe"
printf 'int main() { return 0; }\n' > "$tmp/probe/main.cpp"
printf 'cmake_minimum_required(VERSION 3.20)\nproject(probe CXX)\nadd_executable(probe main.cpp)\n' > "$tmp/probe/CMakeLists.txt"
if ! cmake -S "$tmp/probe" -B "$tmp/probe-build" >"$tmp/probe.log" 2>&1 ||
   ! cmake --build "$tmp/probe-build" >>"$tmp/probe.log" 2>&1 ||
   ! "$tmp/probe-build/probe" >>"$tmp/probe.log" 2>&1; then
    echo 'SETUP PROBLEM: CMake or C++ toolchain probe failed'; exit 2
fi
mkdir -p "$tmp/include"
cat > "$tmp/include/book.h" <<'H'
#pragma once
int best_bid();
H
cat > "$tmp/book.cpp" <<'CPP'
#include "book.h"
int best_bid() { return 101; }
CPP
cat > "$tmp/main.cpp" <<'CPP'
#include "book.h"
int main() { return best_bid() - 101; }
CPP
# Substitute the learner's tokens into a modern target-based skeleton.
cat > "$tmp/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.20)
project(M7C2 CXX)
add_library(orderbook book.cpp)
target_include_directories(orderbook PUBLIC include)
$tcf(orderbook PUBLIC cxx_std_20)
add_executable(matcher main.cpp)
$tll(matcher $scope orderbook)
EOF

if ! err=$(cmake -S "$tmp" -B "$tmp/build" 2>&1); then
    echo "MISS: cmake configure failed:"; echo "$err" | grep -iE 'error' | head -3; exit 1
fi
if err=$(cmake --build "$tmp/build" 2>&1); then
    echo "HIT (target-based CMakeLists configures+builds; -std from cxx_std_20, link via '$tll $scope')"; exit 0
else
    echo "MISS: build failed:"; echo "$err" | grep -iE 'error' | head -3; exit 1
fi
