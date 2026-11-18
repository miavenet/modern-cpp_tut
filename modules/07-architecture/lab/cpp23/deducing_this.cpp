// deducing_this.cpp -- deducing this (C++23): what type does Self become?
// clang++ -std=c++23 modules/07-architecture/lab/cpp23/deducing_this.cpp -o "${TMPDIR:-/tmp}/dthis" && "${TMPDIR:-/tmp}/dthis"
#include <cstdio>
#include <type_traits>

struct Derived;

struct Base {
    template <class Self>
    void who(this Self&& self);            // explicit object parameter, deduced
    void fixed(this Base& self);           // explicit object parameter, fixed type
};
struct Derived : Base {};

template <class Self>
void Base::who(this Self&&) {
    if constexpr (std::is_same_v<Self, Derived&>) std::puts("who:   Self = Derived&");
    else if constexpr (std::is_same_v<Self, Base&>) std::puts("who:   Self = Base&");
    else std::puts("who:   Self = something else");
}
void Base::fixed(this Base&) { std::puts("fixed: bound to Base&"); }

int main() {
    Derived d;
    Base b;
    d.who();     // the derived type is deduced
    b.who();
    d.fixed();   // Derived binds to Base& through the usual derived-to-base conversion
}
