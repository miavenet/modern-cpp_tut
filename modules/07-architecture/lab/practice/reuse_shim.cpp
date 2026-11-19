// reuse_shim.cpp - malloc/free wrappers in a separate file, so the optimizer cannot delete the probe's allocations.
// Build and run from the tutorial root; the program goes to your temp directory:
//   bash modules/07-architecture/lab/practice/reuse_check.sh clang++   (or: g++)
#include <cstdlib>
#include <cstddef>
extern "C" void* request_bytes(std::size_t n) { return std::malloc(n); }
extern "C" void retire_bytes(void* p) { std::free(p); }
