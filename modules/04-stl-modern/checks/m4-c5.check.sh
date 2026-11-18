#!/bin/bash
root=$(cd "$(dirname "$0")/../../.." && pwd)
exec python3 "$root/tools/check_runner.py" "$(dirname "$0")/m4-c5.impl.sh"
