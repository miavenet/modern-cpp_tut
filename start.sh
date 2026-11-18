#!/bin/bash
# Start or resume the Modern C++ tutorial.
# Picks the module with due reviews (or the first unstarted one) and runs the
# Offline driver. Progress is saved after every card; pause with :quit.
exec python3 "$(dirname "$0")/launch.py" --resume "$@"
