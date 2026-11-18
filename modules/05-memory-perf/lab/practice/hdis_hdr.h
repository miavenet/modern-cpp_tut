#include <new>
struct Padded { alignas(std::hardware_destructive_interference_size) long v; };
