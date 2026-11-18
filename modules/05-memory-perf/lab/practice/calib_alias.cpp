// calib_alias.cpp - call both calibration functions in place (in == out), under AddressSanitizer.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O1 -g -fsanitize=address modules/05-memory-perf/lab/practice/calib.cpp modules/05-memory-perf/lab/practice/calib_alias.cpp -o "${TMPDIR:-/tmp}/calib_alias" && "${TMPDIR:-/tmp}/calib_alias"
#include <cstdint>
#include <cstddef>
#include <cstdio>
std::size_t calib_stop(const std::int32_t*,std::int32_t*,std::size_t,std::int32_t);
std::size_t calib_all(const std::int32_t*,std::int32_t*,std::size_t,std::int32_t);
int main(){
 std::int32_t a[]={0,INT32_MIN,2},b[]={0,INT32_MIN,2};
 std::fprintf(stderr,"in-place stop returned %zu\n",calib_stop(a,a,3,7));
 std::fprintf(stderr,"in-place all returned %zu\n",calib_all(b,b,3,7));
}
