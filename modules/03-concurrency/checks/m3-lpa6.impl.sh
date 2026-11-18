#!/bin/bash
# Check for M3-LPA6: the memory order in the responder's busy-poll (completion).
# Learner's answer on stdin: one memory-order name for the blank in
#   while (turn.load(std::memory_order____) != 1) {}
# Graded by behaviour: 2,000 hand-offs under ThreadSanitizer must deliver every
# payload in order with no data race reported. relaxed (race) and consume
# (orders only dependent reads) are refused with their reason.
set -u
answer=$(head -n 1 | sed 's/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//')
name=${answer#std::}; name=${name#memory_order::}; name=${name#memory_order_}
case "$name" in
    acquire) ;;
    relaxed) echo 'MISS: relaxed ends the loop but orders nothing; ThreadSanitizer reports a race on payload'; exit 1 ;;
    consume) echo 'MISS: consume orders only reads that depend on the loaded value; payload does not depend on turn'; exit 1 ;;
    seq_cst|acq_rel|release) echo 'MISS: the weakest order that still pairs with the pinger release store is wanted (acquire)'; exit 1 ;;
    '') echo 'MISS: give one memory-order name'; exit 1 ;;
    *) echo 'MISS: answer with one memory-order name such as acquire'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
cat >"$tmp/t.cpp" <<CPP
#include <atomic>
#include <thread>
std::atomic<int> turn{0}, done{0};
int payload = 0;
int main() {
    int bad = 0;
    std::thread responder([&] {
        for (int i = 1; i <= 2000; ++i) {
            while (turn.load(std::memory_order_$name) != i) {}
            if (payload != i * 7) ++bad;
            done.store(i, std::memory_order_release);
        }
    });
    for (int i = 1; i <= 2000; ++i) {
        payload = i * 7;
        turn.store(i, std::memory_order_release);
        while (done.load(std::memory_order_acquire) != i) {}
    }
    responder.join();
    return bad;
}
CPP
if ! clang++ -std=c++20 -O1 -fsanitize=thread "$tmp/t.cpp" -o "$tmp/t" >"$tmp/build.log" 2>&1; then
    echo 'SETUP PROBLEM: known-good specimen does not build under ThreadSanitizer'; head -3 "$tmp/build.log"; exit 2
fi
if perl -e 'alarm 60; exec @ARGV' "$tmp/t" >"$tmp/run.log" 2>&1; then
    echo 'HIT (2,000 hand-offs, every payload in order, no data race under ThreadSanitizer)'; exit 0
fi
echo 'MISS: the hand-off failed or ThreadSanitizer reported a race'; exit 1
