// atomic_copies.cpp - three ways to copy 64 bytes from a live source: memcpy, atomic_ref bytes, atomic_ref words.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra -S modules/03-concurrency/lab/practice/atomic_copies.cpp -o -
#include <atomic>
#include <cstdint>
#include <cstring>
struct alignas(8) Blob { unsigned char b[64]; };
void copy_plain(Blob& dst, const Blob& src) { std::memcpy(&dst, &src, sizeof(Blob)); }
void copy_bytes(Blob& dst, Blob& src) {
    for (int i = 0; i < 64; ++i)
        dst.b[i] = std::atomic_ref<unsigned char>(src.b[i]).load(std::memory_order_relaxed);
}
struct Words { std::uint64_t w[8]; };  // same 64 bytes, as eight words
void copy_words(Words& dst, Words& src) {
    for (int i = 0; i < 8; ++i)
        dst.w[i] = std::atomic_ref<std::uint64_t>(src.w[i]).load(std::memory_order_relaxed);
}
