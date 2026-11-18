// hive_check.cpp - which standard library is this, and does it ship std::hive?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++2c -O2 modules/07-architecture/lab/practice/hive_check.cpp -o "${TMPDIR:-/tmp}/hive_check" && "${TMPDIR:-/tmp}/hive_check"
#include <cstdio>
#include <version>
int main() {
#ifdef _LIBCPP_VERSION
  std::printf("standard library: libc++ _LIBCPP_VERSION=%d\n", _LIBCPP_VERSION);
#endif
#ifdef __GLIBCXX__
  std::printf("standard library: libstdc++ __GLIBCXX__=%d\n", __GLIBCXX__);
#endif
  std::printf("__cplusplus=%ld\n", (long)__cplusplus);
#ifdef __cpp_lib_hive
  std::printf("__cpp_lib_hive=%ld\n", (long)__cpp_lib_hive);
#else
  std::printf("__cpp_lib_hive not defined: this library does not ship std::hive\n");
#endif
#if __has_include(<hive>)
  std::printf("__has_include(<hive>)=1\n");
#else
  std::printf("__has_include(<hive>)=0\n");
#endif
}
