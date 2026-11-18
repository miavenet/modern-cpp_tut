# 2 independent fadd chains; 2 elements per iteration; one pointer bump
ldr  d16, [x0, #0]
ldr  d17, [x0, #8]
fadd d0, d0, d16
fadd d1, d1, d17
add  x0, x0, #16
