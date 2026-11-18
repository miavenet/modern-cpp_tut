# Module 07 lesson 04 lab: a two-target project (library + executable).
# From the tutorial root (use solution in place of starter for the answer):
#   cmake -S modules/07-architecture/lab/cmake/starter -B "${TMPDIR:-/tmp}/cmake-lab"
#   cmake --build "${TMPDIR:-/tmp}/cmake-lab" && "${TMPDIR:-/tmp}/cmake-lab/matcher/matcher"
cmake_minimum_required(VERSION 3.20)
project(Matcher CXX)

add_subdirectory(orderbook)   # defines the library target `orderbook`
add_subdirectory(matcher)     # defines the executable target `matcher`
