// bindings.cpp - structured bindings over named map entries: copy or reference.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -Wall -Wextra modules/04-stl-modern/lab/bindings.cpp -o "${TMPDIR:-/tmp}/bindings" && "${TMPDIR:-/tmp}/bindings"
#include <cstdio>
#include <map>
#include <string>

struct Book {
    static inline int copies = 0;
    int updates = 0;
    Book() = default;
    Book(const Book& o) : updates(o.updates) { ++copies; }
    void update() { ++updates; }
};

int main() {
    std::map<std::string, Book> books;
    books["AAPL"]; books["MSFT"];                   // two default Books

    for (auto [symbol, book] : books) book.update();   // copies each pair
    std::printf("by value:     copies=%d, AAPL updates=%d\n", Book::copies, books["AAPL"].updates);

    for (auto& [symbol, book] : books) book.update();  // refers into the map
    std::printf("by reference: copies=%d, AAPL updates=%d\n", Book::copies, books["AAPL"].updates);

    auto [it, inserted] = books.try_emplace("AAPL");  // pair<iterator, bool>
    std::printf("inserted AAPL again? %s\n", inserted ? "yes" : "no");
}
