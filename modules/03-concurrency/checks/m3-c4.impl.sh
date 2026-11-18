#!/bin/bash
# Check for M3-C4: coroutine machinery identification (keyword-based).
# The blanks are NAMES filled into English sentences, not a code template, so
# there is nothing to compile. We check all three components are named, with a
# contradiction guard against confusing coroutines with inheritance/virtuals.
# Learner's answer on stdin.
answer=$(cat)

has_promise=$(echo "$answer" | grep -c   'promise_type')
has_handle=$(echo "$answer"  | grep -c   'coroutine_handle')
has_await=$(echo "$answer"   | grep -ciE 'await')                   # awaitable/awaiter

has_wrong=$(echo "$answer" | grep -ciE 'virtual|inheritance')

if [ "$has_wrong" -gt 0 ]; then
    echo "MISS: coroutine machinery is not virtual dispatch / inheritance"
    exit 1
fi

if [ "$has_promise" -gt 0 ] && [ "$has_handle" -gt 0 ] && [ "$has_await" -gt 0 ]; then
    echo "HIT"
    exit 0
else
    echo "MISS: name the promise_type, the coroutine_handle, and the awaitable/awaiter"
    exit 1
fi
