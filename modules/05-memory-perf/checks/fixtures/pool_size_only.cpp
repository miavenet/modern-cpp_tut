// Adversarial fixture: same-size requests with different alignment must not share a pool class.
// Module 05 lab, REFERENCE SOLUTION. Try lab/starter/pool_lab.cpp first.
//
// Build and run from the tutorial root (the binary goes to your temp folder):
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/05-memory-perf/lab/solution/pool_lab.cpp \
//       -o "${TMPDIR:-/tmp}/pool_lab" && "${TMPDIR:-/tmp}/pool_lab"
//
// The first ordinary request binds the pool (size AND alignment); zero-byte
// requests go upstream without binding. The pool recycles
// blocks of exactly that class through an intrusive free list, and forwards
// every other request to the upstream resource unchanged.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <list>
#include <memory_resource>
#include <new>
#include <vector>

struct Order {
    std::uint64_t id;
    double px;
    std::uint32_t qty;
    char side;
};

// Upstream resource that counts every call that reaches it, then uses the heap.
class counting_resource : public std::pmr::memory_resource {
public:
    std::size_t allocations = 0;
    std::size_t deallocations = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        ++allocations;
        return std::pmr::new_delete_resource()->allocate(bytes, align);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        ++deallocations;
        std::pmr::new_delete_resource()->deallocate(p, bytes, align);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override {
        return this == &o;
    }
};

constexpr bool kPoolReady = true;   // step 4 (TODO(M5-C6) in the starter)

class fixed_pool : public std::pmr::memory_resource {
public:
    fixed_pool(std::size_t blocks_per_chunk, std::pmr::memory_resource* upstream)
        : per_chunk_(blocks_per_chunk), upstream_(upstream) {}
    fixed_pool(const fixed_pool&) = delete;
    fixed_pool& operator=(const fixed_pool&) = delete;
    ~fixed_pool() override {
        // Give every chunk back with the same size and alignment we asked for.
        for (const chunk& c : chunks_) upstream_->deallocate(c.p, c.bytes, c.align);
    }

    std::size_t bound_size() const { return size_; }
    std::size_t bound_align() const { return align_; }
    std::size_t stride() const { return stride_; }
    std::size_t blocks_per_chunk() const { return per_chunk_; }
    std::size_t forwarded() const { return forwarded_; }

private:
    struct node { node* next; };
    struct chunk { void* p; std::size_t bytes; std::size_t align; };

    // Is this request in the one size/alignment class the pool serves?
    // The first ordinary request picks the class (self-binding); zero-byte requests bypass this test.
    bool in_class(std::size_t bytes, std::size_t align) {
        if (size_ == 0) {
            size_ = bytes;
            align_ = align;
            block_align_ = std::max(align, alignof(node));
            std::size_t raw = std::max(bytes, sizeof(node));
            stride_ = (raw + block_align_ - 1) / block_align_ * block_align_;
        }
        return bytes == size_; // FAULT: ignores alignment
    }

    // Step 1 (TODO(M5-C6) in the starter): one upstream chunk, sliced into blocks.
    void refill() {
        std::size_t bytes = stride_ * per_chunk_;
        void* p = upstream_->allocate(bytes, block_align_);
        chunks_.push_back({p, bytes, block_align_});   // bookkeeping uses the global heap
        auto* base = static_cast<std::byte*>(p);
        for (std::size_t i = 0; i < per_chunk_; ++i) push(base + i * stride_);
    }

    // Step 2 (TODO(M5-C6) in the starter): pop the head of the free list.
    void* pop() {
        node* n = free_;
        free_ = n->next;
        return n;
    }

    // Step 3 (TODO(M5-C6) in the starter): push a block onto the head of the free list.
    void push(void* p) {
        node* n = ::new (p) node;   // start a node's lifetime in the raw block
        n->next = free_;
        free_ = n;
    }

    void* do_allocate(std::size_t bytes, std::size_t align) override {
        if (!in_class(bytes, align) || !kPoolReady) {
            ++forwarded_;
            return upstream_->allocate(bytes, align);
        }
        if (free_ == nullptr) refill();   // the only path that touches upstream
        return pop();
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        if (!in_class(bytes, align) || !kPoolReady) {   // same test as do_allocate
            upstream_->deallocate(p, bytes, align);
            return;
        }
        push(p);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override {
        return this == &o;   // memory from one pool cannot be freed by another
    }

    std::size_t per_chunk_;
    std::pmr::memory_resource* upstream_;
    std::size_t size_ = 0, align_ = 0, block_align_ = 0, stride_ = 0;
    std::size_t forwarded_ = 0;
    node* free_ = nullptr;
    std::vector<chunk> chunks_;
};

int main() {
    constexpr int kRounds = 3;
    constexpr std::uint64_t kOrders = 1000;
    counting_resource upstream;
    std::size_t after[kRounds + 1] = {};
    std::size_t forwarded = 0;
    {
        fixed_pool pool(256, &upstream);
        std::pmr::list<Order> book(&pool);
        for (int round = 1; round <= kRounds; ++round) {
            for (std::uint64_t i = 0; i < kOrders; ++i)
                book.push_back(Order{i, 100.0 + double(i), 10, 'B'});
            book.clear();   // every node goes back to the pool
            after[round] = upstream.allocations;
            std::printf("round %d: %llu orders in and out, upstream allocations so far = %zu\n",
                        round, (unsigned long long)kOrders, after[round]);
        }
        std::printf("pool bound to %zu-byte, %zu-aligned requests (block stride %zu, "
                    "%zu blocks per chunk); sizeof(Order) = %zu\n",
                    pool.bound_size(), pool.bound_align(), pool.stride(),
                    pool.blocks_per_chunk(), sizeof(Order));
        forwarded = pool.forwarded();
        std::printf("requests the pool forwarded upstream: %zu\n", forwarded);
    }
    std::printf("after the pool is destroyed: upstream allocations = %zu, deallocations = %zu\n",
                upstream.allocations, upstream.deallocations);

    bool flat = after[kRounds] == after[1];
    bool balanced = upstream.allocations == upstream.deallocations;
    if (flat && forwarded == 0 && balanced)
        std::puts("RESULT: PASS - no upstream allocations after round 1, every chunk returned");
    else if (!kPoolReady)
        std::puts("RESULT: TODO - every node still goes upstream; finish the four TODO steps in fixed_pool");
    else
        std::puts("RESULT: FAIL - upstream count still grows or chunks leak; re-check refill/pop/push");
    return (flat && forwarded == 0 && balanced) ? 0 : 1;
}
