#!/bin/bash
# rdt_probe.sh - read-only exposure probe: does this Linux box show cache-partitioning (RDT) flags and a resctrl interface?
# Run from the tutorial root, on Linux: bash modules/06-hft-patterns/lab/practice/rdt_probe.sh
# It changes nothing. An empty result shows what this environment exposes, not what the CPU can do.
echo "uname: $(uname -srm)"
echo "-- x86 RDT flags in /proc/cpuinfo (count of matching flag names):"
grep -m1 -E '^(flags|Features)' /proc/cpuinfo | tr ' ' '\n' | grep -c -E '^(rdt_a|cat_l3|cat_l2|cdp_l3|mba|cqm_llc|cqm_occup_llc)$'
echo "-- first cpuinfo feature line (truncated):"; grep -m1 -E '^(flags|Features)' /proc/cpuinfo | cut -c1-120
echo "-- resctrl in /proc/filesystems:"; grep -c resctrl /proc/filesystems
echo "-- /sys/fs/resctrl:"; ls /sys/fs/resctrl 2>&1 | head -3
