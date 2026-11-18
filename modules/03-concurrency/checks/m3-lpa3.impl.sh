#!/bin/bash
# Check for M3-LPA3: the responder's busy-poll. Learner's answer on stdin: one C++
# statement that waits, without ever sleeping, until `turn` (a std::atomic<int>) is 1,
# such that the plain-int `payload` the pinger wrote before its release store is
# visible afterwards. Graded by behaviour, never by time: 5,000 hand-offs under
# ThreadSanitizer must deliver every payload in order with no data race reported.
# A fence after a relaxed spin is NOT accepted: ThreadSanitizer cannot grade fences.
# Waits that can sleep (wait, sleep, yield, mutex, condition variable) are refused.
set -u
answer=$(head -n 1 | sed 's://.*$::; s:/\*.*\*/::g; s/^[[:space:]]*//; s/[[:space:]]*$//; s/^`//; s/`$//; s/[[:space:]]*$//')
if [ -z "$answer" ]; then echo 'MISS: give one C++ statement'; exit 1; fi
case "$answer" in
    *'#'*|*'\'*|*'asm'*) echo 'MISS: give one plain C++ statement'; exit 1 ;;
    *consume*) echo 'MISS: memory_order_consume orders only reads that depend on the loaded value; payload does not depend on turn, so it is not ordered (use acquire)'; exit 1 ;;
    *wait*|*sleep*|*yield*|*mutex*|*condition*|*lock*) echo 'MISS: a busy-poll never asks the OS to put the thread to sleep (no wait, sleep, yield or lock)'; exit 1 ;;
esac
# Comments were removed above, so a note such as "// spin, do not wait" is harmless.
# One statement: a for-header has two semicolons of its own; anything else may have
# at most one (the closing one, or one inside a braced body).
body=${answer%;}
semis=$(printf '%s' "$body" | tr -cd ';' | wc -c | tr -d ' ')
limit=1
case "$body" in for*'('*) limit=2 ;; esac
if [ "$semis" -gt "$limit" ]; then echo 'MISS: give one statement'; exit 1; fi
# Always built and run under ThreadSanitizer. Forms ThreadSanitizer cannot grade (a fence
# after a relaxed spin: valid C++, but TSan does not model fences and reports a false race)
# are NOT accepted; the card key lists them. Tricks that would blind or leave the probe
# are refused too. The wait is compiled in its own function that sees only `turn`, so it
# cannot touch the probe's payload, counters or loop.
case "$answer" in
    *fence*) echo 'MISS: a fence form is not accepted here (ThreadSanitizer cannot grade fences): use an acquire load as the loop condition'; exit 1 ;;
    *tsan*|*TSAN*|*sanitize*|*Annotate*|*extern*|*'__'*|*'"'*|*dlsym*) echo 'MISS: give a plain wait on turn, with no sanitizer hooks or declarations'; exit 1 ;;
    *return*|*break*|*continue*|*goto*|*throw*|*exit*|*abort*|*terminate*) echo 'MISS: the wait must keep polling until turn == 1; do not leave early'; exit 1 ;;
esac
if ! command -v clang++ >/dev/null 2>&1; then echo 'SETUP PROBLEM: clang++ is missing; run ./setup.sh'; exit 2; fi
tmp=$(mktemp -d) || { echo 'SETUP PROBLEM: cannot create a temporary build directory'; exit 2; }
trap 'rm -rf "$tmp"' EXIT
printf '#include <thread>\nint main() { std::thread t([]{}); t.join(); }\n' >"$tmp/toolchain.cpp"
if ! clang++ -std=c++20 -fsanitize=thread "$tmp/toolchain.cpp" -o "$tmp/toolchain" >"$tmp/toolchain.log" 2>&1 ||
   ! "$tmp/toolchain" >/dev/null 2>&1; then
    echo 'SETUP PROBLEM: this compiler cannot build and run a ThreadSanitizer program'; exit 2
fi
case "$answer" in *';'|*'}') stmt="$answer" ;; *) stmt="$answer;" ;; esac
cat >"$tmp/probe.cpp" <<CPP
#include <atomic>
#include <thread>
constexpr int kRounds = 5000;
std::atomic<int> turn{0};
static void wait_for_turn() {          // the learner's statement sees only turn
    $stmt
}
int main() {
    int payload = 0;                   // plain int: only ordering makes it safe
    int bad = 0;
    std::thread responder([&] {
        for (int i = 1; i <= kRounds; ++i) {
            wait_for_turn();
            if (payload != i) ++bad;
            turn.store(0, std::memory_order_release);
        }
    });
    for (int i = 1; i <= kRounds; ++i) {
        payload = i;
        turn.store(1, std::memory_order_release);
        while (turn.load(std::memory_order_acquire) != 0) {}
    }
    responder.join();
    return bad ? 3 : 0;
}
CPP
san=-fsanitize=thread
if ! clang++ -std=c++20 -O1 -g $san "$tmp/probe.cpp" -o "$tmp/probe" >"$tmp/build.log" 2>&1; then
    echo 'MISS: that statement does not compile as the wait (turn is a std::atomic<int>)'
    grep -m1 'error:' "$tmp/build.log" | sed 's/^.*error: //'
    exit 1
fi
TSAN_OPTIONS='halt_on_error=1 exitcode=66' "$tmp/probe" >"$tmp/run.log" 2>&1 &
pid=$!
( sleep 30; kill -9 "$pid" 2>/dev/null ) >/dev/null 2>&1 &
dog=$!
wait "$pid"
status=$?
kill "$dog" 2>/dev/null; wait "$dog" 2>/dev/null
if grep -q 'ThreadSanitizer: data race' "$tmp/run.log"; then status=66; fi
case $status in
    0) echo 'HIT (5,000 hand-offs, every payload seen in order, ThreadSanitizer clean)'; exit 0 ;;
    66) echo 'MISS: ThreadSanitizer reports a data race on payload: read it only after an acquire (or stronger) load has seen turn == 1; a relaxed load, or a check that does not wait, does not order it'; exit 1 ;;
    137|265) echo 'MISS: the program did not finish: the wait never returned (or stopped polling)'; exit 1 ;;
    3) echo 'MISS: the responder read a payload before the pinger had published it'; exit 1 ;;
    *) echo "MISS: the program failed (exit $status)"; exit 1 ;;
esac
