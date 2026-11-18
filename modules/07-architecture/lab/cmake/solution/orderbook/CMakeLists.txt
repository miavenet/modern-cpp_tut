add_library(orderbook STATIC src/book.cpp)
target_compile_features(orderbook PUBLIC cxx_std_20)

# PUBLIC: book.h is part of the library API, so consumers need this path too.
target_include_directories(orderbook PUBLIC include)

# Release builds of THIS target only add -march=native (code tuned for the
# machine that builds it). The custom build type "Asan" adds AddressSanitizer
# when compiling this target, and (INTERFACE) when linking anything that uses it.
target_compile_options(orderbook PRIVATE
    "$<$<CONFIG:Release>:-march=native>"
    "$<$<CONFIG:Asan>:-fsanitize=address;-g;-fno-omit-frame-pointer>")
target_link_options(orderbook INTERFACE "$<$<CONFIG:Asan>:-fsanitize=address>")
