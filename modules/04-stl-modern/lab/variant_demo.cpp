// variant_demo.cpp - std::variant (C++17) + std::visit with an overload set.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/variant_demo.cpp -o "${TMPDIR:-/tmp}/variant_demo" && "${TMPDIR:-/tmp}/variant_demo"
#include <cstdio>
#include <variant>

struct Heartbeat {};
struct Order  { int qty; double px; };
struct Cancel { int order_id; };

using Message = std::variant<Heartbeat, Order, Cancel>;

template <class... Fs> struct overloaded : Fs... { using Fs::operator()...; };
template <class... Fs> overloaded(Fs...) -> overloaded<Fs...>; // needed in C++17 only

void process(const Message& m) {
    std::visit(overloaded{
        [](const Heartbeat&) { std::puts("heartbeat"); },
        [](const Order& o)   { std::printf("order %d @ %.2f\n", o.qty, o.px); },
        [](const Cancel& c)  { std::printf("cancel #%d\n", c.order_id); },
    }, m);
}

int main() {
    Message m = Order{100, 10.25};
    process(m);
    m = Cancel{7};                       // old Order destroyed, Cancel now active
    process(m);
    std::printf("index()=%zu\n", m.index());
    if (auto* o = std::get_if<Order>(&m)) std::printf("qty %d\n", o->qty);
    else std::puts("get_if<Order> returned nullptr");
    try { (void)std::get<Order>(m); }
    catch (const std::bad_variant_access&) { std::puts("get<Order> threw bad_variant_access"); }
}
