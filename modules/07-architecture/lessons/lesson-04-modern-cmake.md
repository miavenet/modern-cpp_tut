# Lesson 04 — Modern CMake: Settings Belong to Targets

Time: about 40 minutes (12 minutes reading, a 10-minute lab, 8 cards).

**The hook.** In this lesson's test, one line,
`set(CMAKE_CXX_FLAGS "-DLATE_SET")`, placed at the *bottom* of a
`CMakeLists.txt`, changed how a program defined at the *top* was compiled,
while a subdirectory added in between kept the old value. Global CMake
variables follow rules few people can recite. Target-based CMake lets you
stop needing to.

**You will be able to:** lay out a two-target project, choose PRIVATE,
PUBLIC or INTERFACE for each setting, and add a sanitizer build type that
works with both kinds of CMake generator.

**You need from earlier:** lesson 02 (sanitizers). CMake 3.20 or newer; the
course was tested with CMake 4.3.3.

## Pretest — guess first

### Card M7-P2 · Pretest
Q: Old CMake set things globally: `include_directories(...)`,
   `add_definitions(...)`, `set(CMAKE_CXX_FLAGS ...)`. Guess: name one way
   this goes wrong as a repository grows, and what a "target" might do instead.
<details><summary>Answer</summary>
Key:
  - A setting meant for one library reaches others in the same directory
    and in subdirectories added later, and which targets get it depends on
    where the line sits relative to `add_subdirectory()` calls.
  - A target (a library or an executable) carries its own settings. For compile usage requirements such as include directories, PUBLIC and
    INTERFACE propagate to consumers; PRIVATE does not. A static library's
    PRIVATE link dependencies can still reach the final link without
    exposing their compile usage requirements.
</details>

---

### CONCEPT C8  Targets, usage requirements and PRIVATE / PUBLIC / INTERFACE

Your executable builds only because another library leaked an include
directory into its compile command. Move one directory and the build breaks.
Target usage requirements make that dependency explicit: a consumer receives
what the library's interface needs.

**C++98-era CMake, and the surprise.** Directory-level commands and
variables apply by directory, not by target. Two observations from this
lesson's tests with CMake 4.3.3:
- `include_directories(/opt/late_dir)` written *after* `add_executable(a ...)`
  in the same file still reached `a`'s compile line.
- With `set(CMAKE_CXX_FLAGS "-DTOP_SET")` at the top and
  `set(CMAKE_CXX_FLAGS "-DLATE_SET")` at the bottom, target `a` in that file
  compiled with `-DLATE_SET`, but a target in a subdirectory added between
  them compiled with `-DTOP_SET`.

**Modern way.** Each `target_*` command attaches a setting to one target and
says who else gets it. From `cmake --help-command target_include_directories`:
"PRIVATE and PUBLIC items will populate the INCLUDE_DIRECTORIES property of
<target>. PUBLIC and INTERFACE items will populate the
INTERFACE_INCLUDE_DIRECTORIES property". In words:

| compile usage keyword | the target itself | targets that link it |
|---|---|---|
| PRIVATE | yes | no |
| INTERFACE | no | yes |
| PUBLIC | yes | yes |

Settings a consumer receives this way are called *usage requirements*.
`target_link_libraries(matcher PRIVATE orderbook)` is what makes `matcher`
receive `orderbook`'s PUBLIC and INTERFACE settings.

**The lab project** (`modules/07-architecture/lab/cmake/starter/`):

```
CMakeLists.txt              project(Matcher); add_subdirectory(orderbook); add_subdirectory(matcher)
orderbook/CMakeLists.txt    add_library(orderbook src/book.cpp) + its settings
orderbook/include/book.h    int best_bid();
orderbook/src/book.cpp
matcher/CMakeLists.txt      add_executable(matcher main.cpp)
                            target_link_libraries(matcher PRIVATE orderbook)
matcher/main.cpp            #include "book.h"
```

**Lab (about 10 minutes).** Configure and build the starter from the
tutorial root:

```sh
cmake -S modules/07-architecture/lab/cmake/starter -B "${TMPDIR:-/tmp}/cmake-lab"
cmake --build "${TMPDIR:-/tmp}/cmake-lab"
```

First run: `matcher/main.cpp:1:10: fatal error: 'book.h' file not found`.
The line marked `TODO(M7-Q3)` in `orderbook/CMakeLists.txt` is
`target_include_directories(orderbook PRIVATE include)`. Change one word,
run the same `cmake --build` command again (CMake re-reads the edited file
itself), and success is `[100%] Built target matcher`; then
`"${TMPDIR:-/tmp}/cmake-lab/matcher/matcher"` prints `best bid: 101`. The
answer is in `lab/cmake/solution/`. Adding `--verbose` to the build shows
both compile lines: each carries `-std=gnu++20`, which matcher received from
`target_compile_features(orderbook PUBLIC cxx_std_20)`. (It is `gnu++20`,
not `c++20`, because CMake's compiler extensions setting defaults to on.)

**Rule:** put every setting on a target, and mark it PUBLIC only when the
target's own headers make consumers need it.

| setting scope | cost and guarantee | right when | wrong-choice smell |
|---|---|---|---|
| global variable or directory setting | broad reach and order sensitivity | deliberate project-wide policy | unrelated target inherits a flag |
| PRIVATE compile setting | applies to the target itself | implementation-only option | consumer fails to compile a public header |
| PUBLIC compile setting | target and link consumers receive it | header forces the requirement | consumers inherit an internal flag |
| INTERFACE compile setting | only link consumers receive it | header-only requirement | target's own source needs the setting |

Use PUBLIC only for what the public header requires; a consumer compiling
only after a global include path is added is the scope-error smell.

### Card M7-J4 · Apply
Q: In the lab project, `orderbook` now uses a `pricing` library, but only
   `src/book.cpp` includes `pricing.h`; the public `book.h` still declares just
   `int best_bid();`. After an unrelated build error, an AI assistant changed
   the link from PRIVATE to PUBLIC "to be safe". Is the scope right here? What
   would you check first, and what would change your verdict?
   Answer format: three short sentences.
   ```cmake
   # orderbook/CMakeLists.txt
   add_library(pricing src/pricing.cpp)
   target_include_directories(pricing PUBLIC pricing_include)
   add_library(orderbook src/book.cpp)
   target_compile_features(orderbook PUBLIC cxx_std_20)
   target_include_directories(orderbook PUBLIC include)
   target_link_libraries(orderbook PUBLIC pricing)   # was PRIVATE
   ```
<details><summary>Answer</summary>
Key:
  - Not here: both versions build and print `best bid: 101`, but PUBLIC also hands `pricing`'s include directory to `matcher`, so `matcher` can start including `pricing.h` without ever declaring that dependency.
  - First check which headers `book.h` includes, because PUBLIC is right only when the public header needs `pricing`.
  - If `book.h` includes `pricing.h`, consumers also need pricing's include directory, so expose that dependency with PUBLIC.
</details>

### Card M7-C2 · Fix it
Q: Fill blanks A, B and C so `matcher` links `orderbook` and inherits its C++20
   requirement, with no global commands:
   ```cmake
   add_library(orderbook book.cpp)
   target_include_directories(orderbook PUBLIC include)
   ______A______(orderbook PUBLIC cxx_std_20)
   add_executable(matcher main.cpp)
   ______B______(matcher ___C___ orderbook)
   ```
   Answer format: one line, three items separated by commas, in the order A, B, C.
<details><summary>Answer</summary>
Key:
  - `target_compile_features, target_link_libraries, PRIVATE`: A requests
    C++20 for `orderbook` and, being PUBLIC, for its consumers; B links
    `matcher` to `orderbook` and passes on its usage requirements; C is a scope
    keyword, PRIVATE because nothing links to `matcher` (PUBLIC also builds).
</details>

### Card M7-Q3 · Recall
Q: In the lab, `orderbook` had `target_include_directories(orderbook PRIVATE
   include)` and `matcher`, which links it, could not find `book.h`.
   (a) For each of PRIVATE, INTERFACE and PUBLIC, say who receives the setting.
   (b) Which keyword fixes the lab, and why?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) PRIVATE: only the target itself. INTERFACE: only targets that link
    it. PUBLIC: both.
  - (b) PUBLIC: `orderbook` compiles with `book.h` itself, and `book.h` is
    part of its interface, so targets that link it need the directory too.
</details>

### Card M7-G8 · Spot the trap
Q: To "optimise everything", a teammate writes
   `set(CMAKE_CXX_FLAGS "-O3 -march=native")` near the top of the root
   `CMakeLists.txt`. Your debug and sanitizer builds now also get these flags.
   (a) In one sentence, why? (b) What is the target-based fix?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `CMAKE_CXX_FLAGS` is a variable applied to every C++ compile in the
    directory and in subdirectories added after it is set, for every build
    type; it knows nothing about individual targets or configurations.
  - (b) Attach the flag to the one target and one configuration that wants
    it, with a generator expression:
    `target_compile_options(orderbook PRIVATE "$<$<CONFIG:Release>:-march=native>")`.
    Note that `-march=native` tunes code for the machine that builds it.
</details>

### Card M7-INT3 · Choose and explain
Q: Pick one library you own. In a short paragraph: (a) which of its settings
   should be PUBLIC and which PRIVATE, and (b) how does that prevent a
   consumer from being built with a language standard older than your headers
   need?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) PUBLIC: what the library's public headers force on users, such as the
    include directory of those headers, a needed `cxx_std_20` feature, or a
    define the headers test. PRIVATE: what only its own `.cpp` files need,
    such as extra warnings, internal include paths, implementation-only
    dependencies.
  - (b) A PUBLIC `target_compile_features(lib PUBLIC cxx_std_20)` travels to
    every target that links `lib`, so each consumer compiles with at least
    C++20 (in the lab, matcher received `-std=gnu++20`), whatever order the
    directories are added in.
</details>

---

### CONCEPT C9  Generator expressions and build types (including a sanitizer build)

**Why configuration needs another boundary.** A Release-only option applied
globally also reaches tests whose purpose is to expose mistakes. A generator
expression attaches the option to one target and one configuration, including
when the configuration is chosen at build time.

**Generator expressions.** `$<...>` is evaluated when CMake writes the build
files, separately for each configuration: the manual says they "are evaluated
during build system generation to produce information specific to each build
configuration". `$<$<CONFIG:Asan>:-fsanitize=address>` expands to the flag
only in the `Asan` configuration, and to nothing otherwise.

**A sanitizer build type**, from `lab/cmake/solution/orderbook/CMakeLists.txt`:

```cmake
target_compile_options(orderbook PRIVATE
    "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:Asan>:-fsanitize=address;-g;-fno-omit-frame-pointer>")
target_link_options(orderbook INTERFACE "$<$<CONFIG:Asan>:-fsanitize=address>")
```

`matcher/CMakeLists.txt` adds the same compile options to `matcher`. ASan
must be on the link line too: the INTERFACE link option puts it on the link
of every executable that uses `orderbook`.

**Choosing the configuration depends on the generator.** A single-config
generator (Unix Makefiles, Ninja) fixes one configuration at configure time;
a multi-config generator (Ninja Multi-Config, Xcode, Visual Studio) picks one
at build time. What this lesson observed:

| what you run | result |
|---|---|
| `cmake -S ... -B b -DCMAKE_BUILD_TYPE=Asan`, then `cmake --build b` | ASan in compile and link lines; `otool -L` shows the ASan runtime |
| single-config, `cmake --build b --config Asan` | `--config` ignored: no `-fsanitize` anywhere |
| `-G "Ninja Multi-Config"`, then `--config Asan` | fails: `build-Asan.ninja: No such file or directory` |
| same, configured with `-DCMAKE_CONFIGURATION_TYPES="Debug;Release;Asan"` | builds with ASan |

Release builds already get `-O3 -DNDEBUG` from CMake's defaults on this
toolchain (seen in the verbose output), so the example adds only
`-march=native`, and only to `orderbook`.

**Old minimum versions.** CMake 4.3.3 refused
`cmake_minimum_required(VERSION 2.8)`: "Compatibility with CMake < 3.5 has
been removed from CMake." The same message suggests
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` "to try configuring anyway", which
worked in the test.

**The trap.** `if(CMAKE_BUILD_TYPE STREQUAL "Release")` around a flag: it is
decided at configure time, and a multi-config generator has no single build
type then.

**Rule:** express per-configuration settings as generator expressions on
targets, and select the configuration the way your generator expects.

### Card M7-Q4 · Apply
Q: A static library `lib` must build two ways without editing files: `Release` with
   `-march=native`, and a custom `Asan` build type with AddressSanitizer.
   (a) Write the target commands (compile and link). (b) How do you select
   `Asan` with a single-config generator, and what extra step does a
   multi-config generator need?
   Answer format: one line; separate labeled parts with semicolons.

<details><summary>Answer</summary>
Key:
  - (a) `target_compile_options(lib PRIVATE "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:Asan>:-fsanitize=address;-g;-fno-omit-frame-pointer>")` and
    `target_link_options(lib INTERFACE "$<$<CONFIG:Asan>:-fsanitize=address>")`
    (executables that link `lib` also need the compile options if their own
    code should be instrumented).
  - (b) Single-config: configure a separate build directory with
    `-DCMAKE_BUILD_TYPE=Asan` (`--config` at build time is ignored).
    Multi-config: list it when configuring,
    `-DCMAKE_CONFIGURATION_TYPES="Debug;Release;Asan"`, then build with
    `--config Asan`.
</details>

### Card M7-G3 · Spot the trap
Q: An old project configures after you pass
   `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. Has it been modernised? What must you
   check before raising its declared minimum?
   Answer format: two short sentences.

<details><summary>Answer</summary>
Key:
  - No. The override selects newer policy behavior; it does not update the
    project. Read the affected policy changes, build and test the supported
    configurations, then declare a minimum version you have actually tested.
</details>

---

## Closing

Remember: settings belong on targets; PUBLIC means "my users need this too";
generator expressions pick per-configuration flags; a custom build type is
chosen with `CMAKE_BUILD_TYPE` on single-config generators and must be listed
in `CMAKE_CONFIGURATION_TYPES` on multi-config ones. Cards you missed come
back tomorrow. If you have not yet done it, lesson 05 (the LPA lens on
benchmarks) is the last one.
