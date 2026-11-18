# 1 independent fadd chains; 1 elements per iteration; one pointer bump
ldr  d16, [x0, #0]
fadd d0, d0, d16
add  x0, x0, #8
