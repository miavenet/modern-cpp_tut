// ifc_fix.cpp - the build-time choice as a template argument.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/02-type-system/lab/practice/ifc_fix.cpp -o "${TMPDIR:-/tmp}/ifc_fix" && "${TMPDIR:-/tmp}/ifc_fix"; echo "exit $?"
template <bool Big> int scale(int qty) {
  if constexpr (Big) return qty * 100;
  return qty;
}
int main() { return scale<true>(3) == 300 ? 0 : 1; }
