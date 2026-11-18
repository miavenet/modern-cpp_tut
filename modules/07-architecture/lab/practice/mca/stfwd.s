# one dependency chain through memory: store the counter, load it back, add 1
str  x1, [x0]
ldr  x2, [x0]
add  x1, x2, #1
