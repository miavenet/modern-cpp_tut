// lto_observe.cpp - an empty observe(), in its own file so the caller cannot see its body without LTO.
// Build and run from the tutorial root; the program goes to your temp directory:
//   see lto_sum.cpp for the two build lines
void observe(const int*) {}   // does nothing, but the caller's file cannot see that
