#!/bin/bash
# clamp_census.sh file.s   (AArch64 listing)
# per function: predicated vector stores (SVE st1b/st1h/st1w/st1d with a p register),
# plain vector stores (str/stp of a q register or str of a z register; NEON st1 lists),
# conditional branches.
awk '
/^_?_Z[A-Za-z0-9_]*:/ { f=$1; sub(/:.*/,"",f); o[++k]=f; p[f]=0; n[f]=0; b[f]=0; next }
f=="" { next }
/^[ \t]*[.#;]/ { next }
{
  if ($0 ~ /^[ \t]*st1[bhwd][ \t]+\{?[ \t]*z[0-9]+\.[bhsd][ \t]*\}?, *p[0-9]/) p[f]++
  else if ($0 ~ /^[ \t]*(str|stur|stp)[ \t]+[qz][0-9]/ || $0 ~ /^[ \t]*st1[ \t]+\{/) n[f]++
  if ($0 ~ /^[ \t]*(b\.[a-z]+|cbz|cbnz|tbz|tbnz)[ \t]/) b[f]++
}
END { for (i=1;i<=k;i++) printf "%-22s predicated-vector-stores %2d  plain-vector-stores %2d  cond-br %3d\n", o[i], p[o[i]], n[o[i]], b[o[i]] }' "$1"
