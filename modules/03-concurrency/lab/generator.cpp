// generator.cpp - a minimal hand-written C++20 generator (no <generator> needed).
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/03-concurrency/lab/generator.cpp -o "${TMPDIR:-/tmp}/generator" && "${TMPDIR:-/tmp}/generator"
#include <coroutine>
#include <cstdio>
#include <cstdlib>
#include <new>

static int frames_allocated = 0;

struct IntGen {
    struct promise_type {                       // the compiler looks for this name
        int current = 0;
        IntGen get_return_object() {
            return IntGen{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; } // start paused
        std::suspend_always final_suspend() noexcept { return {}; }   // stay paused at end
        std::suspend_always yield_value(int v) noexcept {             // runs on co_yield v
            current = v;
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() { std::abort(); }
        static void* operator new(std::size_t n) {                    // count frame allocations
            ++frames_allocated;
            return ::operator new(n);
        }
        static void operator delete(void* p) noexcept { ::operator delete(p); }
    };

    std::coroutine_handle<promise_type> h;      // non-owning pointer to the frame
    explicit IntGen(std::coroutine_handle<promise_type> handle) : h(handle) {}
    IntGen(const IntGen&) = delete;
    ~IntGen() { if (h) h.destroy(); }           // this wrapper owns the frame

    bool next() { h.resume(); return !h.done(); } // run to the next co_yield
    int value() const { return h.promise().current; }
};

IntGen count_to(int n) {
    for (int i = 1; i <= n; ++i)
        co_yield i;                             // suspend here, hand i out
}

int main() {
    IntGen g = count_to(3);                     // body has not run yet
    while (g.next())
        std::printf("got %d\n", g.value());
    std::printf("frames allocated: %d\n", frames_allocated);
}
