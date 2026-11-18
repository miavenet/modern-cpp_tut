// to_unsigned.cpp - how each target lowers double -> unsigned 64-bit and double -> signed 64-bit.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ --target=arm64-apple-macos -std=c++20 -O2 -Wall -Wextra -S modules/02-type-system/lab/practice/to_unsigned.cpp -o -
//   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu modules/02-type-system/lab/practice/to_unsigned.cpp -o -
//   clang++ -std=c++20 -O2 -Wall -Wextra -S --target=x86_64-linux-gnu -mavx512f modules/02-type-system/lab/practice/to_unsigned.cpp -o -
// How does each target lower double -> unsigned 64-bit and double -> signed 64-bit?
// No headers, so the x86-64 cross compile needs no sysroot. On LP64 targets
// (Linux, macOS) unsigned long and long are 64 bits.
static_assert(sizeof(unsigned long) == 8, "LP64 expected");
unsigned long to_u64(double d) { return static_cast<unsigned long>(d); }
long          to_i64(double d) { return static_cast<long>(d); }
