// recv_wakeups.cpp - blocking vs spinning recv: the receiver thread's context switches and empty polls (Linux only).
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra -pthread modules/06-hft-patterns/lab/practice/recv_wakeups.cpp -o "${TMPDIR:-/tmp}/recv_wakeups" && for i in 1 2 3 4 5; do "${TMPDIR:-/tmp}/recv_wakeups"; "${TMPDIR:-/tmp}/recv_wakeups" spin; done
//   strace -f -c -e trace=recvfrom "${TMPDIR:-/tmp}/recv_wakeups" spin   (a separate, traced run)
// Receiver waits for 200 datagrams that arrive about 0.2 ms apart.
// "block": recv() sleeps until data arrives. "spin": recv(MSG_DONTWAIT) in a loop.
// Report the receiver thread's own context-switch counts (Linux RUSAGE_THREAD).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <thread>
constexpr int N = 200;
int main(int argc, char** argv) {
    bool spin = argc > 1 && std::strcmp(argv[1], "spin") == 0;
    int rx = socket(AF_INET, SOCK_DGRAM, 0), tx = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(rx, (sockaddr*)&a, sizeof a) != 0) { perror("bind"); return 1; }
    socklen_t len = sizeof a; getsockname(rx, (sockaddr*)&a, &len);
    long empty_polls = 0, vol = 0, invol = 0; int got = 0;
    std::thread receiver([&] {
        rusage r0{}, r1{}; getrusage(RUSAGE_THREAD, &r0);
        char buf[64];
        while (got < N) {
            ssize_t k = recv(rx, buf, sizeof buf, spin ? MSG_DONTWAIT : 0);
            if (k > 0) ++got;
            else if (k < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) ++empty_polls;
            else { perror("recv"); return; }
        }
        getrusage(RUSAGE_THREAD, &r1);
        vol = r1.ru_nvcsw - r0.ru_nvcsw; invol = r1.ru_nivcsw - r0.ru_nivcsw;
    });
    for (int i = 0; i < N; ++i) { sendto(tx, "x", 1, 0, (sockaddr*)&a, sizeof a); usleep(200); }
    receiver.join();
    std::printf("mode=%-5s received=%d voluntary_switches=%ld involuntary_switches=%ld empty_polls=%ld\n",
                spin ? "spin" : "block", got, vol, invol, empty_polls);
}
