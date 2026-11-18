// spsc_ring.hpp -- reference solution for the module 06 SPSC lab.
// One producer thread calls try_push; one consumer thread calls try_pop.
#pragma once
#include <atomic>
#include <bit>          // std::has_single_bit (C++20)
#include <cstddef>
#include <cstdint>
#include <type_traits>

// 128 bytes = the cache-line size `sysctl hw.cachelinesize` reported on the
// Apple silicon Mac used to test this course. Check your own machine.
inline constexpr std::size_t kCacheLine = 128;

template <typename T, std::size_t N>
class SpscRing {
    static_assert(std::has_single_bit(N), "N must be a power of two");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "try_push/try_pop are noexcept");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    static constexpr std::uint64_t kMask = N - 1;

    // head_: next slot the producer writes. Only the producer stores it.
    // tail_: next slot the consumer reads. Only the consumer stores it.
    alignas(kCacheLine) std::atomic<std::uint64_t> head_{0};
    alignas(kCacheLine) std::atomic<std::uint64_t> tail_{0};
    alignas(kCacheLine) T slots_[N]{};

public:
    // Producer thread only.
    bool try_push(const T& v) noexcept {
        const std::uint64_t h = head_.load(std::memory_order_relaxed);  // my own cursor
        const std::uint64_t t = tail_.load(std::memory_order_acquire);  // pairs with try_pop's release
        if (h - t == N) return false;                                   // full
        slots_[h & kMask] = v;
        head_.store(h + 1, std::memory_order_release);                  // publish the slot
        return true;
    }

    // Consumer thread only.
    bool try_pop(T& out) noexcept {
        const std::uint64_t t = tail_.load(std::memory_order_relaxed);  // my own cursor
        const std::uint64_t h = head_.load(std::memory_order_acquire);  // pairs with try_push's release
        if (h == t) return false;                                       // empty
        out = slots_[t & kMask];
        tail_.store(t + 1, std::memory_order_release);                  // hand the slot back
        return true;
    }
};
