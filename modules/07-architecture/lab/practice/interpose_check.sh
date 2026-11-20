#!/bin/bash
# interpose_check.sh - does a preloaded adjust() replace the plugin's own? GCC and Clang, normal and -Wl,-Bsymbolic-functions builds.
# Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/interpose_check.sh
# Builds go to a scratch folder in your temp directory. LD_PRELOAD is set only for the single command it prefixes.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd); W=${TMPDIR:-/tmp}/interpose_check
mkdir -p "$W"; cd "$W"
g++ --version | head -1
uname -m
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_lib.cpp" -o loose.so
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_lib.cpp" -Wl,-Bsymbolic-functions -o bound.so
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_hook.cpp" -o hook.so
g++ -std=c++20 -O2 -Wall -Wextra "$D/interpose_driver.cpp" -ldl -o driver
echo "GCC, no preload:"; ./driver ./loose.so; ./driver ./bound.so
echo "GCC, with preload:"; LD_PRELOAD=./hook.so ./driver ./loose.so; LD_PRELOAD=./hook.so ./driver ./bound.so
for f in loose.so bound.so; do echo "nm $f:"; nm -D --defined-only "$f"; done
g++ -std=c++20 -O2 -Wall -Wextra -fPIC -fno-semantic-interposition -shared "$D/interpose_lib.cpp" -o nsi.so
g++ -std=c++20 -O0 -Wall -Wextra -fPIC -shared "$D/interpose_lib.cpp" -o unoptimized.so
echo "GCC -fno-semantic-interposition, without then with preload:"; ./driver ./nsi.so; LD_PRELOAD=./hook.so ./driver ./nsi.so
echo "GCC -O0, with preload:"; LD_PRELOAD=./hook.so ./driver ./unoptimized.so
clang++ --version | head -1
clang++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_lib.cpp" -o cl_loose.so
clang++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_lib.cpp" -Wl,-Bsymbolic-functions -o cl_bound.so
clang++ -std=c++20 -O2 -Wall -Wextra -fPIC -shared "$D/interpose_hook.cpp" -o cl_hook.so
clang++ -std=c++20 -O2 -Wall -Wextra "$D/interpose_driver.cpp" -ldl -o cl_driver
echo "Clang, no preload:"; ./cl_driver ./cl_loose.so; ./cl_driver ./cl_bound.so
echo "Clang, with preload:"; LD_PRELOAD=./cl_hook.so ./cl_driver ./cl_loose.so; LD_PRELOAD=./cl_hook.so ./cl_driver ./cl_bound.so
for f in nsi.so cl_loose.so cl_bound.so; do echo "nm $f:"; nm -D --defined-only "$f"; done
for f in loose.so nsi.so cl_loose.so; do echo "answer() in $f:"; objdump -d --disassemble=answer "$f" | grep -E '^ +[0-9a-f]+:'; done
g++ -O2 -fPIC -Q --help=optimizers | grep semantic-interposition
