add_executable(matcher main.cpp)
target_link_libraries(matcher PRIVATE orderbook)   # inherits orderbook's PUBLIC settings
target_compile_options(matcher PRIVATE "$<$<CONFIG:Asan>:-fsanitize=address;-g;-fno-omit-frame-pointer>")
