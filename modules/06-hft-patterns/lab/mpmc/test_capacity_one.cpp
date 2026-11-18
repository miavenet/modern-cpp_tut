// Intentionally fails to compile: capacity one cannot distinguish full from empty.
#include "mpmc_ring.hpp"
MpmcRing<int, 1> invalid_ring;
