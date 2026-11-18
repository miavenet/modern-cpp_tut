#!/bin/bash
# usage: census.sh file.s
# Per function in a compiler listing (AArch64 or x86 AT&T syntax) print:
#   vec-lines   lines that name a SIMD register
#   vec-stores  stores of a whole SIMD register to memory (masked ones included)
#   masked      stores under a mask (x86 vpmaskmov*, or AVX-512 {%k})
#   cond-br     conditional branch instructions
awk '
/^_?_Z[A-Za-z0-9_]*:/ { f=$1; sub(/:.*/,"",f); o[++k]=f; v[f]=0; s[f]=0; m[f]=0; b[f]=0; next }
f=="" { next }
/^[ \t]*[.#;]/ { next }
{
  line=$0
  if (line ~ /%[xyz]mm[0-9]/ || line ~ /[ \t,\[]v[0-9]+[.\[]/ || line ~ /[ \t,]q[0-9]+/) v[f]++
  # AArch64 whole-register stores
  if (line ~ /^[ \t]*(str|stur|stp)[ \t]+q[0-9]/ || line ~ /^[ \t]*st[1-4][ \t]/) s[f]++
  # x86 AT&T: source register first, memory operand last
  if (line ~ /^[ \t]*v?mov(dq[au][0-9]*|up[sd]|ap[sd])[ \t]+%[xyz]mm[0-9]+, *[^%]*\(/) { s[f]++; if (line ~ /\{%k[0-9]\}/) m[f]++ }
  if (line ~ /^[ \t]*vpmaskmov[dq][ \t]+%[xyz]mm[0-9]+, *%[xyz]mm[0-9]+, *[^%]*\(/) { s[f]++; m[f]++ }
  if (line ~ /^[ \t]*(b\.[a-z]+|cbz|cbnz|tbz|tbnz)[ \t]/) b[f]++
  if (line ~ /^[ \t]*j[a-z]+[ \t]/ && line !~ /^[ \t]*jmp/) b[f]++
}
END { for (i=1;i<=k;i++) printf "%-26s vec-lines %3d  vec-stores %2d  masked %d  cond-br %3d\n", o[i], v[o[i]], s[o[i]], m[o[i]], b[o[i]] }' "$1"
