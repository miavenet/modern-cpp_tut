#!/bin/bash
# M2-C4 has three ordered blanks; the public wrapper also validates them.
answer=$(cat)
IFS=, read -r base refined rank extra <<< "$answer"
base=$(printf '%s' "$base" | tr -d '[:space:]')
refined=$(printf '%s' "$refined" | tr -d '[:space:]')
rank=$(printf '%s' "$rank" | tr -d '[:space:]')
if [ "$base" != Message ]; then
    echo "MISS: blank 1: expected Message; got ${base:-missing}"
    exit 1
fi
if [ "$refined" != MarketDataMessage ]; then
    echo "MISS: blank 2: expected MarketDataMessage; got ${refined:-missing}"
    exit 1
fi
if [ "$rank" != constrained ] || [ -n "${extra:-}" ]; then
    echo "MISS: blank 3: expected constrained; got ${rank:-missing}"
    exit 1
fi
echo 'HIT (ordered concept names and overload rank)'
