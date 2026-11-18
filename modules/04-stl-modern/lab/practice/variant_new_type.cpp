// variant_new_type.cpp - add one alternative and every visitor without a catch-all stops compiling.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -fsyntax-only modules/04-stl-modern/lab/practice/variant_new_type.cpp                          (compiles)
//   clang++ -std=c++20 -fsyntax-only -DADD_TYPE modules/04-stl-modern/lab/practice/variant_new_type.cpp               (fails: visitor not exhaustive)
//   clang++ -std=c++20 -fsyntax-only -DADD_TYPE -DCATCH_ALL modules/04-stl-modern/lab/practice/variant_new_type.cpp   (compiles again: safety lost)
//   clang++ -std=c++20 -DADD_TYPE -DCATCH_ALL -DRUN modules/04-stl-modern/lab/practice/variant_new_type.cpp -o "${TMPDIR:-/tmp}/variant_new_type" && "${TMPDIR:-/tmp}/variant_new_type"
//     (runs it: prints the area of a Rect and of a Triangle)
#include <variant>
struct Circle { double r; }; struct Rect { double w, h; };
#ifdef ADD_TYPE
struct Triangle { double b, h; };
using ShapeV = std::variant<Circle, Rect, Triangle>;
#else
using ShapeV = std::variant<Circle, Rect>;
#endif
template <class... F> struct overloaded : F... { using F::operator()...; };
double area(const ShapeV& s) {
  return std::visit(overloaded{[](const Circle& c) { return 3.14159 * c.r * c.r; },
                               [](const Rect& r) { return r.w * r.h; }
#ifdef CATCH_ALL
                               , [](const auto&) { return 0.0; }
#endif
                    }, s);
}
#if defined(RUN) && defined(ADD_TYPE)
#include <cstdio>
int main() {
  std::printf("area(Rect{3,4})=%g\n", area(Rect{3, 4}));
  std::printf("area(Triangle{3,4})=%g\n", area(Triangle{3, 4}));
}
#endif
