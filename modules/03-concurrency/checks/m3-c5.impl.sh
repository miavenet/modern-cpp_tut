#!/bin/bash
# Check for M3-C5: std::generator lazy generator (exact blank tokens).
# This portable check does not require a library that provides <generator>.
# Learner's answer on stdin.
answer=$(cat)

IFS=, read -r b1 b2 extra <<< "$answer"
[[ -z "$extra" && "$b1" =~ ^[[:space:]]*generator[[:space:]]*$ &&
   "$b2" =~ ^[[:space:]]*co_yield[[:space:]]*$ ]] || {
    echo "MISS: answer exactly generator, co_yield in that order"
    exit 1
}
echo HIT
exit 0
