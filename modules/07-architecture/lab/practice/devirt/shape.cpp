// shape.cpp - the only implementation of Shape (commands in use.cpp).
#include "shape.h"
long Square::area(long side) { acc += side * side; return acc; }
static Square g;
Shape& make_shape() { return g; }
Square& make_square() { return g; }
