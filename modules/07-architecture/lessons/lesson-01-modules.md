# Lesson 01 — C++20 Modules: `import` Instead of Copy-and-Paste

Time: about 35 minutes (12 minutes reading, a 5-minute build, 8 cards).

**Module 07 adds** the tools around the code: modules (this lesson),
sanitizers (02), benchmarks and a few C++23 library features (03), and
target-based CMake (04). **It assumes** modules 01 to 03 and that you can
compile from a terminal. Every command runs from the tutorial root and writes
its output to `${TMPDIR:-/tmp}`, never into the course folder.

**The hook.** Somewhere a C header says `#define max(a,b) ...`, and from then
on `std::max(x, y)` in your file stops compiling. `#include` pastes text, so
the macro comes along. A C++20 module is compiled once and imported, and its
macros stay home. You will see that happen in a 15-line build.

**You will be able to:** write a module interface, build and import it with
the commands that work on this course's toolchain, and say honestly what
build-tool support exists today.

## Pretest — guess first

### Card M7-P1 · Pretest
Q: Guess: (a) what does `#include "book.h"` do before the compiler proper
   sees your file? (b) How might C++20's `import book;` differ?
<details><summary>Answer</summary>
Key:
  - (a) The preprocessor copies the text of `book.h` (and everything it
    includes) into your file. Each `.cpp` that includes it parses all that text
    again, unless the build uses precompiled headers.
  - (b) A module is compiled once per build into a binary module interface
    (on Clang, a `.pcm` file); importers load that instead of re-parsing text.
    Macros defined inside the module do not leak into the importer.
</details>

---

### CONCEPT C1  A module interface, `export`, and the two-step build

In their 2015 modules proposal, Gabriel Dos Reis and Pavel Curtis argued
that textual inclusion had reached its limits. Named modules separate an
exported C++ interface from the importer's preprocessor state: importing
the module does not import its macros. Predict which names survive the
boundary in the example below.

**C++98 way.** A header plus `#include`:

```cpp
// book.h                               // main.cpp
#ifndef BOOK_H                          #include "book.h"
#define BOOK_H                          int main() { return add(2, 3); }
int add(int a, int b);
#endif
```

It hurts in three ways: every includer re-parses the text, include order can
matter, and every macro in the header lands in the includer.

**Modern way (C++20).** `modules/07-architecture/lab/modules/math_core.cppm`:

```cpp
module;                      // global module fragment: plain #includes go here
#include <cstdio>
#define SECRET_MACRO 1       // a macro defined inside the module
export module math_core;     // this file is the interface of module math_core

export int add(int a, int b) { return a + b; }   // exported: importers see it
int helper() { return 42; }                      // not exported: module-private
export void hello() { std::puts("hello from math_core"); }
```

The importer, `main.cpp`, starts with `import math_core;` and calls `add` and
`hello`; it also checks whether `SECRET_MACRO` is defined.

**Words.** An interface unit's module declaration is `export module name;`; an
implementation unit's is `module name;`. An optional global module fragment
beginning with `module;` can precede either declaration. An implementation
unit can hold definitions of things the interface declares. One interface file is enough
for a small module; an implementation unit lets you change function bodies
without changing the interface. The compiled interface is a *BMI*, which
Clang's documentation defines as "the precompiled result of an importable
module unit"; Clang writes it as a `.pcm` file.

**Build it (Apple clang 21, tested).** Two steps: precompile the interface,
then compile the importer against it. Apple clang needs `-fcxx-modules`; its
driver passes `-fno-cxx-modules` by default (seen with `clang++ -###`), and
without the flag the first line fails with "a type specifier is required for
all declarations".

```sh
T="${TMPDIR:-/tmp}"; M=modules/07-architecture/lab/modules
clang++ -std=c++20 -fcxx-modules --precompile "$M/math_core.cppm" -o "$T/math_core.pcm"
clang++ -std=c++20 -fcxx-modules "$M/main.cpp" -fmodule-file=math_core="$T/math_core.pcm" \
  "$T/math_core.pcm" -o "$T/mod_app" && "$T/mod_app"
```

```
add(2, 3) = 5
hello from math_core
SECRET_MACRO not visible here
```

Calling `helper()` from an importer fails: "use of undeclared identifier
'helper'".

**Does a comment edit force importers to rebuild?** On the test machine,
editing only a comment in `math_core.cppm` and precompiling again produced a
`.pcm` with the same size but different bytes (`cmp` reported a difference).
A build tool that rebuilds importers when the BMI changes will rebuild them.

**The trap.** "Modules make builds faster, full stop." They can avoid
repeated parsing; measure clean and incremental builds with your own
toolchain before promising anything.

**Rule:** `export module` names the interface, `export` marks what importers
may use, and `import` consumes the compiled interface.

| boundary | cost and guarantee | right when | wrong-choice smell |
|---|---|---|---|
| header | textual include is parsed by each includer unless cached; macros travel with it | legacy toolchain or small stable header | include-order surprises |
| named module | build must produce a BMI before importers; exports are explicit and macros stay inside | supported compiler and dependency scanner | manual commands work but the build graph cannot scan imports |

Choose a small leaf component for a module trial; a build tool that cannot
discover its imports is the smell to stop the wider migration.

### Card M7-C1 · Fix it
Q: Fill the three blanks to define and use a module for an order book:
   ```cpp
// Fragment: fill the blanks in this exercise.
   // order_book.cppm
   ___ module order_book;           // blank 1
   ___ class OrderBook {            // blank 2
   public:
       void process() {}
   };
   // main.cpp
   ___ order_book;                  // blank 3
   int main() { OrderBook book; book.process(); }
   ```
   Answer format: one line, three keywords separated by commas, blanks in order.
<details><summary>Answer</summary>
Key:
  - `export, export, import`: `export module` declares the interface unit,
    `export class` makes `OrderBook` visible to importers, and `import`
    consumes the module.
</details>

### Card M7-Q1 · Recall
Q: (a) How does the module declaration of an interface unit differ from
   that of an implementation unit? (b) Does every module need both kinds of file?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) An interface unit's module declaration is `export module name;`; an
    implementation unit's is `module name;`. An optional global module
    fragment beginning with `module;` can precede either declaration.
  - (b) No. A single interface unit can hold everything; an implementation
    unit is optional and lets you change bodies without changing the interface.
</details>

### Card M7-INT1 · Choose and explain
Q: Pick a header that almost every file in your codebase includes. In two or
   three sentences: what gets recompiled when you edit a comment in it today,
   and what would you have to measure before claiming modules make that faster?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - Today every translation unit that includes the header (directly or
    indirectly) is recompiled, because its text changed.
  - With a module, the interface is recompiled; whether importers are rebuilt
    depends on whether the build tool sees the BMI as changed. On the test
    machine a comment edit changed the `.pcm` bytes, so importers would be
    rebuilt there.
  - Measure clean and incremental build times with your compiler and build tool
    before and after.
</details>

---

### CONCEPT C2  Macros, reachability, and today's tool support

**Why visibility needs its own rule.** A function can return a type without
giving every importer permission to spell every name from the header that
defined it. That boundary prevents an accidental dependency on the module's
implementation includes.

**Macros stay inside.** In the build above, `SECRET_MACRO` was defined in
the module's global module fragment, and the importer printed
`SECRET_MACRO not visible here`. To tame a legacy header that defines `max`,
include it in the global module fragment of a module that exports clean
functions; importers get the functions, not the macro.

**Visible versus reachable.** `lab/modules/book.cppm` includes `<vector>` in
its global module fragment and exports
`std::vector<int> best_prices()`. An importer that never includes `<vector>`:

```cpp
auto v = best_prices();          // compiles: the type is reachable
std::printf("%zu levels, best %d\n", v.size(), v[0]);   // prints "3 levels, best 101"
std::vector<int> w;              // error: the NAME is not visible here
```

Clang's error for the last line: "missing '#include <__vector/vector.h>';
'vector' must be declared before it is used". Commands are in the comments of
`use_ok.cpp` and `use_bad.cpp`.

**Tool support, dated (checked November 2026).**
- Apple clang 21 builds named modules with the two-step commands above.
- CMake 4.3.3 with AppleClang 21 and Ninja refused: "the compiler does not
  provide a way to discover the import graph dependencies", and generation
  failed. CMake's own manual (`cmake --help-manual cmake-cxxmodules`) lists
  scanning support for MSVC 14.34+, LLVM/Clang 16+ and GCC 14+, with the Ninja
  and Visual Studio generators. Those combinations are untested here.

**Rule:** named modules do not leak their macros, and importers use exported
names. Fine print: a type may be reachable through an export even when its
name is not visible. Check your compiler and build tool before migrating.

### Card M7-J1 · Apply
Q: A legacy header defines a `max` macro that breaks `std::max`, and also
   `FEED_MAX_LEVELS`, which `book.cpp` uses. An AI assistant wrapped the
   header in a module and switched `book.cpp` to `import`. Is the change right
   here? What would you check first, and what would change your verdict?
   Answer format: three short sentences.
   ```cpp
   // feed.cppm (separate file)
   module;
   #define FEED_MAX_LEVELS 10                  // stands in for #include "legacy_feed.h"
   #define max(a, b) ((a) > (b) ? (a) : (b))   // the macro you want to hide
   export module feed;
   export int clamp_levels(int n) { return n > FEED_MAX_LEVELS ? FEED_MAX_LEVELS : n; }
   // book.cpp (separate file)
   #include <algorithm>
   import feed;                                // was: #include "legacy_feed.h"
   int levels[FEED_MAX_LEVELS];                // sizes the book from the header's macro
   int main() { return std::max(clamp_levels(12), 0) == 10 ? 0 : 1; }
   ```
<details><summary>Answer</summary>
Key:
  - Not as it stands: the module keeps `max` away from `std::max`, but it also keeps `FEED_MAX_LEVELS` away, so `book.cpp` stops compiling ("use of undeclared identifier 'FEED_MAX_LEVELS'").
  - First check which macros the importers actually use from the legacy header, by searching for them or compiling each importer against the module.
  - If importers needed only functions, this wrapper would be right; for a macro that is part of the interface, export a constant such as `export constexpr int feed_max_levels = FEED_MAX_LEVELS;` and use that name instead.
</details>

### Card M7-Q2 · Apply
Q: A legacy C header `legacy_macro.h` defines `#define max(a,b) ...`, which
   breaks `std::max` wherever it is included. In two sentences: how can a
   C++20 module keep the macro away from your code, and where in the module
   file does the `#include` go?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - Write a module that includes the header and exports clean functions or
    types; code that does `import` gets those exports but not the macro,
    because macros do not leak out of a module.
  - The `#include` goes in the global module fragment: after `module;` and
    before the `export module name;` line.
</details>

### Card M7-G1 · Spot the trap
Q: Your team wants to switch a large CMake project to C++20 named modules next
   sprint, building on macOS with Apple clang. In two sentences: what did this
   lesson's test show about that toolchain, and what is the safe first step?
   Answer format: one line.

<details><summary>Answer</summary>
Key:
  - Apple clang 21 compiled named modules by hand (`-fcxx-modules`,
    `--precompile`, `-fmodule-file=`), but CMake 4.3.3 refused to generate a
    modules build with AppleClang because it could not scan module dependencies.
  - Start with one small leaf library, on a compiler and generator that CMake's
    manual lists as supported, and measure build times before going further.
</details>

### Card M7-G2 · Spot the trap
Q: Module `book` includes `<vector>` in its global module fragment and exports
   `std::vector<int> best_prices();`. An importer never includes `<vector>`.
   (a) Does `auto v = best_prices(); v.size();` compile? (b) Does
   `std::vector<int> w;` compile? Answer yes or no for each, with one reason.
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) Yes: the returned type is reachable through the exported function, so
    the importer can use the object and its members.
  - (b) No: the name `std::vector` is not visible in the importer; Clang says
    "'vector' must be declared before it is used". Include or import what you
    name.
</details>

---

## Closing

Remember: `export module` plus `export` define what importers see; macros
stay inside the module; using a reachable type is not the same as naming it.
Check support for your exact compiler and build tool. Cards you missed come
back tomorrow. Next: lesson 02, sanitizers.
