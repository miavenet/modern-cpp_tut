// book.cppm -- exports a function whose return type is std::vector<int>.
module;
#include <vector>
export module book;

export std::vector<int> best_prices() { return {101, 100, 99}; }
