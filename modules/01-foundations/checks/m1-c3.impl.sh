#!/bin/bash
# Check for M1-C3: auto type deduction (keyword-based).
# The blanks are deduced TYPE NAMES (recall), not code to fill, so there is
# nothing to compile. We check the three load-bearing deductions and guard the
# classic trap (thinking c is std::string).
# Learner's answer on stdin.
answer=$(cat)

has_int=$(echo "$answer"    | grep -c   'int')                       # a -> int
has_double=$(echo "$answer" | grep -c   'double')                    # b -> double
has_charptr=$(echo "$answer"| grep -ciE 'char\s*\*|c.?string|char\s*pointer|const char')  # c -> const char*

# Contradiction: claiming c deduces to std::string.
has_wrong=$(echo "$answer" | grep -ciE 'c.*(is|:|=|->).*std::string|c.*deduces.*string')

if [ "$has_wrong" -gt 0 ]; then
    echo "MISS: c deduces to const char* (a string literal), NOT std::string"
    exit 1
fi

if [ "$has_int" -gt 0 ] && [ "$has_double" -gt 0 ] && [ "$has_charptr" -gt 0 ]; then
    echo "HIT"
    exit 0
else
    echo "MISS: need int, double, and const char* (the tricky one) for a, b, c"
    exit 1
fi
