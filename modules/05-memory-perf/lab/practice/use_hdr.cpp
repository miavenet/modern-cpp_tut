// use_hdr.cpp - the interference constant used in a header (hdis_hdr.h).
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   g++ -std=c++20 -Wall -c modules/05-memory-perf/lab/practice/use_hdr.cpp -o "${TMPDIR:-/tmp}/use_hdr.o"   (GCC)
#include "hdis_hdr.h"
int main(){ Padded p{}; return (int)p.v; }
