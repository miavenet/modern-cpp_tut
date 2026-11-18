// trivial_pass.cpp - an 8-byte handle passed by value to a function in another TU.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=arm64-apple-macos
//   clang++ -std=c++20 -O2 -S modules/01-foundations/lab/practice/trivial_pass.cpp -o - --target=x86_64-linux-gnu
// Which variants travel in a register, and which are spilled to the caller's stack
// and passed by address? Compare the call sites. No headers, so it also compiles
// for a non-native target without a sysroot.
struct P  { long h; };                                  // no user-written special members
struct D  { long h; ~D(); };                            // user-provided destructor (defined elsewhere)
struct DF { long h; ~DF() = default; };                 // defaulted on its first declaration
struct OD { long h; OD(long v) : h(v) {} OD(const OD&); };  // copy ctor defaulted out of line, below
OD::OD(const OD&) = default;                            // user-provided: not defaulted on first declaration
struct DL { long h; DL(long v) : h(v) {} DL(const DL&) = delete; DL(DL&&) = delete; };  // all deleted
static_assert(__is_trivially_copyable(P) && __is_trivially_copyable(DF));
static_assert(!__is_trivially_copyable(D) && !__is_trivially_copyable(OD));
void sinkP(P); void sinkD(D); void sinkDF(DF); void sinkOD(OD); void sinkDL(DL);
void callP(long x)  { sinkP(P{x}); }
void callD(long x)  { sinkD(D{x}); }
void callDF(long x) { sinkDF(DF{x}); }
void callOD(long x) { sinkOD(OD{x}); }
void callDL(long x) { sinkDL(DL{x}); }   // C++17 guaranteed elision: no copy or move needed
