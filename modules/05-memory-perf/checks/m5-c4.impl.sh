#!/bin/bash
answer=$(cat)
IFS=, read -r a b c extra <<< "$answer"
a=$(printf '%s' "$a" | xargs)
b=$(printf '%s' "$b" | xargs)
c=$(printf '%s' "$c" | xargs)
if [ "$a" = execution ] && [ "$b" = execution ] &&
   { [ "$c" = par ] || [ "$c" = par_unseq ]; } && [ -z "$extra" ]; then
    echo 'HIT (ordered blanks)'
    exit 0
fi
echo 'MISS: give the header, namespace, and parallel policy in order'
exit 1
