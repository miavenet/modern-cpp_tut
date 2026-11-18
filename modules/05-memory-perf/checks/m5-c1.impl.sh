#!/bin/bash
# Ordered blank check: only the learner's exact fill-ins earn a hit.
answer=$(cat)
IFS=, read -r b1 b2 b3 extra <<< "$answer"
b1=$(printf '%s' "$b1" | xargs)
b2=$(printf '%s' "$b2" | xargs)
b3=$(printf '%s' "$b3" | xargs)
if [ "$b1" = "monotonic_buffer_resource" ] && [ "$b2" = "string" ] && [ "$b3" = "resource" ] && [ -z "$extra" ]; then
    echo "HIT (ordered blanks)"
    exit 0
fi
echo "MISS: give the 3 exact blanks in order, separated by commas"
exit 1
