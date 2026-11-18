// mpmc_ring.hpp -- a teaching adaptation of the Vyukov-style bounded MPMC
// algorithm as implemented by libcds (Copyright 2006-2018 Maxim Khizhinsky).
// Boost Software License 1.0; see LICENSES/Boost-1.0.txt. Modified for this
// tutorial: fixed-size storage and simplified copy-assignment interface.
// Original queue implementation:
// Copyright (c) 2010-2011 Dmitry Vyukov. All rights reserved.
// Simplified BSD licence: see LICENSES/BSD-2-Clause.txt.
// Reading material for module 06
// lesson 01; test_mpmc.cpp exercises it. Not lock-free in the formal sense:
// a thread stalled between claiming a slot and publishing it blocks others.
#pragma once
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

template <typename T, std::size_t N>
class MpmcRing {
    static_assert(N >= 2 && std::has_single_bit(N),
                  "N must be a power of two and at least 2");
    static_assert(std::is_nothrow_copy_assignable_v<T>);
    struct Cell {
        std::atomic<std::uint64_t> seq;  // which "lap" of the ring this cell is ready for
        T data;
    };
    alignas(128) Cell cells_[N];
    alignas(128) std::atomic<std::uint64_t> enqueue_pos_{0};
    alignas(128) std::atomic<std::uint64_t> dequeue_pos_{0};

public:
    MpmcRing() {
        for (std::size_t i = 0; i < N; ++i) cells_[i].seq.store(i, std::memory_order_relaxed);
    }

    bool try_push(const T& v) noexcept {
        std::uint64_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            Cell& c = cells_[pos & (N - 1)];
            std::uint64_t seq = c.seq.load(std::memory_order_acquire);
            auto diff = static_cast<std::int64_t>(seq - pos);
            if (diff == 0) {                       // cell is free for this lap: try to claim it
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    { c.data = v; c.seq.store(pos + 1, std::memory_order_release); return true; }
                // CAS failed: pos now holds the current value; loop and retry
            } else if (diff < 0) {
                return false;                      // cell still holds last lap's value: full
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);  // another producer won; reload
            }
        }
    }

    bool try_pop(T& out) noexcept {
        std::uint64_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            Cell& c = cells_[pos & (N - 1)];
            std::uint64_t seq = c.seq.load(std::memory_order_acquire);
            auto diff = static_cast<std::int64_t>(seq - (pos + 1));
            if (diff == 0) {
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    { out = c.data; c.seq.store(pos + N, std::memory_order_release); return true; }
            } else if (diff < 0) {
                return false;                      // nothing published here yet: empty
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }
};
