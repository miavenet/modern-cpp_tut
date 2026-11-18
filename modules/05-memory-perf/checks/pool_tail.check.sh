#!/bin/bash
root=$(cd "$(dirname "$0")/../../.." && pwd)
exec python3 "$root/tools/check_runner.py" "$(dirname "$0")/pool_tail.impl.sh"
