#!/bin/sh
# usage: count_branches.sh file.s   (AArch64 assembly from clang or GCC)
# Prints, per function, how many conditional-branch instructions it contains.
# clang writes "b.ne"; GCC writes "bne". Both spellings are matched.
awk '
/^_?_Z[A-Za-z0-9_]*:/ { fn=$1; sub(/:.*/,"",fn); order[++n]=fn; c[fn]=0; next }
/^[ \t]+(b\.?(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)|cbz|cbnz|tbz|tbnz)[ \t]/ { c[fn]++ }
/^[ \t]+(csel|cset|csetm|csinc|cinc|ccmp|ccmn)[ \t]/ { s[fn]++ }
END { for (i=1;i<=n;i++) printf "%-28s cond_branches=%d  select_or_ccmp=%d\n", order[i], c[order[i]], s[order[i]]+0 }' "$1"
