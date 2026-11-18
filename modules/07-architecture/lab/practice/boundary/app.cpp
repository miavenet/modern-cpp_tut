// app.cpp - "benchmark" side: calls the accessor in a hot loop.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra [-flto] -c modules/07-architecture/lab/practice/boundary/lib.cpp -o "${TMPDIR:-/tmp}/lib.o"
//   clang++ -std=c++20 -O2 -Wall -Wextra [-flto] -c modules/07-architecture/lab/practice/boundary/app.cpp -o "${TMPDIR:-/tmp}/app.o"
//   clang++ -O2 [-flto] "${TMPDIR:-/tmp}/lib.o" "${TMPDIR:-/tmp}/app.o" -o "${TMPDIR:-/tmp}/app_bin" && "${TMPDIR:-/tmp}/app_bin"
//   objdump -d --no-show-raw-insn "${TMPDIR:-/tmp}/app_bin" | awk '/<_?main>:/{p=1;next} /^$/{p=0} p' | grep -cE '(bl|call).*_?book_spread'
// "Benchmark" side: calls the accessor in a hot loop.
#include <cstdint>
#include <cstdio>
struct Book { std::uint64_t best_bid; std::uint64_t best_ask; };
extern "C" std::uint64_t book_spread(const Book* b);
int main(int argc, char**) {
  Book b{100, 100 + (std::uint64_t)argc};
  std::uint64_t sum = 0;
  for (int i = 0; i < 1000; ++i) { b.best_bid += 1; b.best_ask += 1; sum += book_spread(&b); }
  std::printf("%llu\n", (unsigned long long)sum);
}
