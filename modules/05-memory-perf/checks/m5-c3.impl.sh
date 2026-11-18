#!/bin/bash
# Ordered blank check: only the learner's exact fill-ins earn a hit.
answer=$(cat)
IFS=, read -r b1 b2 extra <<< "$answer"
b1=$(printf '%s' "$b1" | xargs)
b2=$(printf '%s' "$b2" | xargs)
if [ "$b1" = "consteval" ] && [ "$b2" = "[[unlikely]]" ] && [ -z "$extra" ]; then
    echo "HIT (ordered blanks)"
    exit 0
fi
echo "MISS: give the 2 exact blanks in order, separated by commas"
exit 1
