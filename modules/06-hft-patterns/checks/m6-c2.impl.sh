#!/bin/bash
# Check for M6-C2: hot-path constraints (keyword-based).
# The blanks are open-ended design choices ("use a fixed-size array", "use a
# lock-free ___") named in prose, not a single fixed template, so we grade the
# two ideas by keyword with a contradiction guard against the banned tools.
# Learner's answer on stdin.
answer=$(cat)

has_array=$(echo "$answer"    | grep -ci 'array')
has_lockfree=$(echo "$answer" | grep -ciE 'queue|ring|spsc|atomic|lock.?free')

# Contradiction: reaching back for vector/mutex on the hot path.
has_wrong=$(echo "$answer" | grep -ciE 'vector|mutex')

if [ "$has_wrong" -gt 0 ]; then
    echo "MISS: vector (heap) and mutex (blocking) are exactly what the hot path forbids"
    exit 1
fi

if [ "$has_array" -gt 0 ] && [ "$has_lockfree" -gt 0 ]; then
    echo "HIT"
    exit 0
else
    echo "MISS: use a fixed-size array for the symbol and the SPSC ring instead of a mutex"
    exit 1
fi
