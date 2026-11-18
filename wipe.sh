#!/bin/bash
# Wipe stored tutorial progress so the course restarts from the beginning.
#   ./wipe.sh          wipe ALL modules (asks for confirmation)
#   ./wipe.sh 03       wipe only module 03
#   ./wipe.sh --yes    skip the confirmation prompt
exec python3 "$(dirname "$0")/launch.py" --wipe "$@"
