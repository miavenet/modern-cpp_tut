add_library(orderbook STATIC src/book.cpp)
target_compile_features(orderbook PUBLIC cxx_std_20)

# TODO(M7-Q3): matcher includes "book.h" but cannot find it. Change ONE word.
target_include_directories(orderbook PRIVATE include)

# Release builds of THIS target only add -march=native (code tuned for the
# machine that builds it). The custom build type "Asan" adds AddressSanitizer
# when compiling this target, and (INTERFACE) when linking anything that uses it.
target_compile_options(orderbook PRIVATE
    "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:Asan>:-fsanitize=address;-g;-fno-omit-frame-pointer>")
target_link_options(orderbook INTERFACE "$<$<CONFIG:Asan>:-fsanitize=address>")
