#!/bin/bash
# export_check.sh - build one plugin three ways (default visibility, all hidden, hidden + public_value exported) and ask dlsym and nm what is exported.
# Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/export_check.sh
# Builds go to a scratch folder in your temp directory. Prints the loader's dlsym results, then nm's dynamic defined symbols per build.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd); W=${TMPDIR:-/tmp}/export_check
mkdir -p "$W"; cd "$W"
g++ --version | head -1
uname -m
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/export_plugin.cpp" -o plain.so
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -fvisibility=hidden -shared "$D/export_plugin.cpp" -o hidden.so
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -fvisibility=hidden -DEXPORT_API -shared "$D/export_plugin.cpp" -o api.so
g++ -std=c++20 -O2 -Wall -Wextra "$D/export_loader.cpp" -ldl -o loader
./loader
for f in plain.so hidden.so api.so; do echo "nm $f:"; nm -D --defined-only "$f"; done
