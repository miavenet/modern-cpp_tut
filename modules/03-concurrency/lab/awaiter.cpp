// awaiter.cpp - what co_await calls, step by step.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra -O2 modules/03-concurrency/lab/awaiter.cpp -o "${TMPDIR:-/tmp}/awaiter" && "${TMPDIR:-/tmp}/awaiter"
#include <coroutine>
#include <cstdio>
#include <exception>

std::coroutine_handle<> parked;                 // someone must resume us later

struct WaitForData {                            // an awaiter: three member functions
    bool await_ready() const noexcept {         // 1. already done? false = suspend
        std::puts("  await_ready -> false");
        return false;
    }
    void await_suspend(std::coroutine_handle<> h) noexcept { // 2. called after suspending
        std::puts("  await_suspend: parking the handle");
        parked = h;
    }
    int await_resume() const noexcept {         // 3. its result is the co_await value
        std::puts("  await_resume -> 42");
        return 42;
    }
};

struct Task {                                   // minimal fire-and-forget task
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; } // run at once
        std::suspend_never final_suspend() noexcept { return {}; }   // frame freed at end
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
};

Task handler() {
    std::puts("handler: before co_await");
    int v = co_await WaitForData{};
    std::printf("handler: resumed with %d\n", v);
}

int main() {
    handler();                                  // runs until the co_await, then returns
    std::puts("main: handler is suspended; resuming it");
    parked.resume();                            // continue after the co_await
    std::puts("main: done");
}
