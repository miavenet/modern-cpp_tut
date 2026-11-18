#!/bin/bash
root=$(cd "$(dirname "$0")/../../.." && pwd)
answer=$(cat)
if [[ ! "$answer" =~ ^[[:space:]]*atomic_ref[[:space:]]*$ ]]; then
    echo '{"status":"MISS","criteria":["answer exactly the atomic_ref blank"]}'
    echo 'MISS'
    echo 'answer exactly the atomic_ref blank'
    exit 1
fi
printf '%s\n' "$answer" | python3 "$root/tools/check_runner.py" "$(dirname "$0")/m3-c3.impl.sh"
