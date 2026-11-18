# 6 independent fadd chains; 6 elements per iteration; one pointer bump
ldr  d16, [x0, #0]
ldr  d17, [x0, #8]
ldr  d18, [x0, #16]
ldr  d19, [x0, #24]
ldr  d20, [x0, #32]
ldr  d21, [x0, #40]
fadd d0, d0, d16
fadd d1, d1, d17
fadd d2, d2, d18
fadd d3, d3, d19
fadd d4, d4, d20
fadd d5, d5, d21
add  x0, x0, #48
