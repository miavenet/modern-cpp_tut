#!/bin/bash
# Check for M1-C1: move constructor completion (compile-verified)
# Learner's answer on stdin. Extracts the identifier for each blank, fills the
# code template, and compiles it with clang++ -std=c++20 -Wall.
# Compile success => HIT. Blanks: 1) && parameter 2) noexcept 3&4) std::move
answer=$(cat)

b1=$(echo "$answer" | grep -oE '&&' | head -1)                       # rvalue-ref param
b2=$(echo "$answer" | grep -oiE 'noexcept' | head -1)                # noexcept spec
b3=$(echo "$answer" | grep -oiE 'move' | head -1)                    # std::move

[ -z "$b1" ] && { echo "MISS: blank 1 (&& rvalue-reference parameter) not identified"; exit 1; }
[ -z "$b2" ] && { echo "MISS: blank 2 (noexcept specifier) not identified"; exit 1; }
[ -z "$b3" ] && { echo "MISS: blanks 3/4 (std::move) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<CPP
#include <vector>
#include <string>
struct Quote {};
class MarketDataHandler {
    std::vector<Quote> quotes_;
    std::string symbol_;
public:
    MarketDataHandler() = default;
    MarketDataHandler(MarketDataHandler $b1 other) $b2
        : quotes_(std::$b3(other.quotes_)),
          symbol_(std::$b3(other.symbol_)) {}
};
int main() { MarketDataHandler a; MarketDataHandler b(std::move(a)); (void)b; }
CPP

if err=$(clang++ -std=c++20 -Wall -o "$tmp/a.out" "$tmp/t.cpp" 2>&1); then
    echo "HIT (filled-in code compiles)"; exit 0
else
    echo "MISS: filled-in code does not compile:"
    echo "$err" | head -3
    exit 1
fi
