#!/bin/bash
# Check for M7-C1: C++20 named modules (keyword-based).
# NOTE: named modules are build-fragile with plain clang++ invocations (they
# need a two-step precompiled-module build across two translation units), so a
# reliable single-invocation compile check is not practical here. We grade the
# module keywords instead, with a contradiction guard against #include.
# Learner's answer on stdin.
answer=$(cat)

has_export=$(echo "$answer" | grep -c 'export')
has_import=$(echo "$answer" | grep -c 'import')

# Contradiction: falling back to the preprocessor #include model.
has_wrong=$(echo "$answer" | grep -ciE '#\s*include')

if [ "$has_wrong" -gt 0 ]; then
    echo "MISS: modules replace #include -- use export/import, not the preprocessor"
    exit 1
fi

if [ "$has_export" -gt 0 ] && [ "$has_import" -gt 0 ]; then
    echo "HIT"
    exit 0
else
    echo "MISS: export the module interface (export module / export class) and import it to consume"
    exit 1
fi
