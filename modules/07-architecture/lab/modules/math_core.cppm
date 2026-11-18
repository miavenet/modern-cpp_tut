// math_core.cppm -- a module interface unit (C++20).
module;                      // global module fragment: #includes go here
#include <cstdio>
#define SECRET_MACRO 1       // a macro defined inside the module
export module math_core;     // this file defines the module "math_core"

export int add(int a, int b) { return a + b; }   // visible to importers
int helper() { return 42; }                      // not exported: module-private
export void hello() { std::puts("hello from math_core"); }
