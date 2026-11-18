#!/bin/bash
# Check for M7-C3: AddressSanitizer compiler flags (flag-verified)
# Learner's answer on stdin. Blanks: 1) sanitize 2) address.
# We extract the sanitizer name and actually invoke clang++ -fsanitize=<name>
# on a tiny file: a bogus sanitizer name makes the driver error out => MISS.
# (thread/undefined/memory compile fine but are caught by the card RejectIf,
#  so only 'address' both compiles AND survives the contradiction guard.)
answer=$(cat)

has_sanitize=$(echo "$answer" | grep -oiE 'sanitize' | head -1)
name=$(echo "$answer" | grep -oiE 'address|thread|undefined|leak|memory' | head -1 | tr 'A-Z' 'a-z')

[ -z "$has_sanitize" ] && { echo "MISS: blank 1 (-fsanitize=) not identified"; exit 1; }
[ -z "$name" ] && { echo "MISS: blank 2 (sanitizer name, e.g. address) not identified"; exit 1; }

tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/t.cpp" <<'CPP'
int main() {
    int* p = new int(5);
    int r = *p;
    delete p;
    return r - 5;
}
CPP

if err=$(clang++ -std=c++20 -O1 -g -fsanitize="$name" -fno-omit-frame-pointer -c "$tmp/t.cpp" -o "$tmp/t.o" 2>&1); then
    echo "HIT (clang++ -fsanitize=$name accepted and compiles)"; exit 0
else
    echo "MISS: -fsanitize=$name did not compile:"
    echo "$err" | head -3
    exit 1
fi
