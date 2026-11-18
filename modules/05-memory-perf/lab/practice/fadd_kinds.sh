#!/bin/sh
# usage: fadd_kinds.sh file.s   (AArch64 assembly from clang or GCC)
# Per function: how many float adds work on vector registers, how many on scalar
# registers, and how many calls remain (a call means part of the work is elsewhere;
# the stack-protector failure call is not counted).
# Vector adds: Apple syntax "fadd.4s v0, ..." ; GNU syntax "fadd v0.4s, ...".
# faddp (the pairwise add used to fold a vector at the end) is not counted.
awk '
/^_?_Z[A-Za-z0-9_]*:/ { fn=$1; sub(/:.*/,"",fn); order[++n]=fn; next }
/^[ \t]+fadd\.[0-9]+[sd][ \t]/ || /^[ \t]+fadd[ \t]+v[0-9]+\./ { v[fn]++; next }
/^[ \t]+fadd[ \t]+[sd][0-9]+,/ { s[fn]++; next }
/^[ \t]+bl[ \t]/ && !/stack_chk_fail/ { c[fn]++ }
END { for (i=1;i<=n;i++) printf "%-28s vector_fadd=%d  scalar_fadd=%d  calls=%d\n", order[i], v[order[i]]+0, s[order[i]]+0, c[order[i]]+0 }' "$1"
