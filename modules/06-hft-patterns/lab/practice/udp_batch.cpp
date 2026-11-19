// udp_batch.cpp - 64 UDP datagrams over loopback, one sendto each or one sendmmsg, drained with recvmmsg (Linux only).
// Build and run from the tutorial root; the program goes to your temp directory:
//   g++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/udp_batch.cpp -o "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" && strace -f -c -e trace=sendto,sendmmsg,recvmmsg "${TMPDIR:-/tmp}/udp_batch" batch
// Send 64 small UDP datagrams over loopback, one call each or one call total,
// then drain them with recvmmsg and report how many calls each side made.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <chrono>
constexpr int N = 64;
int main(int argc, char** argv) {
    bool batch = argc > 1 && std::strcmp(argv[1], "batch") == 0;
    int rx = socket(AF_INET, SOCK_DGRAM, 0), tx = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = 0;
    if (bind(rx, (sockaddr*)&a, sizeof a) != 0) { perror("bind"); return 1; }
    socklen_t len = sizeof a; getsockname(rx, (sockaddr*)&a, &len);
    char payload[N][32];
    int send_calls = 0;
    if (!batch) {
        for (int i = 0; i < N; ++i) {
            int k = std::snprintf(payload[i], sizeof payload[i], "msg %d", i);
            if (sendto(tx, payload[i], k, 0, (sockaddr*)&a, sizeof a) != k) { perror("sendto"); return 1; }
            ++send_calls;
        }
    } else {
        mmsghdr m[N]; iovec v[N]; std::memset(m, 0, sizeof m);
        for (int i = 0; i < N; ++i) {
            int k = std::snprintf(payload[i], sizeof payload[i], "msg %d", i);
            v[i] = {payload[i], (size_t)k};
            m[i].msg_hdr.msg_iov = &v[i]; m[i].msg_hdr.msg_iovlen = 1;
            m[i].msg_hdr.msg_name = &a; m[i].msg_hdr.msg_namelen = sizeof a;
        }
        int sent = 0;
        while (sent < N) {  // sendmmsg may accept fewer than asked: retry with the rest
            int r = sendmmsg(tx, m + sent, N - sent, 0);
            if (r < 0) { if (errno == EINTR) continue; perror("sendmmsg"); return 1; }
            sent += r; ++send_calls;
        }
    }
    // Receive side: always batched, non-blocking, until all N arrived.
    mmsghdr rm[N]; iovec rv[N]; char rbuf[N][64]; std::memset(rm, 0, sizeof rm);
    for (int i = 0; i < N; ++i) { rv[i] = {rbuf[i], sizeof rbuf[i]}; rm[i].msg_hdr.msg_iov = &rv[i]; rm[i].msg_hdr.msg_iovlen = 1; }
    int got = 0, recv_calls = 0, empty = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);  // UDP can lose datagrams: never wait forever
    while (got < N) {
        int r = recvmmsg(rx, rm + got, N - got, MSG_DONTWAIT, nullptr);
        ++recv_calls;
        if (r > 0) got += r;
        else if (r < 0 && errno == EINTR) continue;                    // retryable
        else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { // retryable: nothing queued yet
            ++empty;
            if (std::chrono::steady_clock::now() > deadline) { std::fprintf(stderr, "timeout: got %d of %d\n", got, N); return 3; }
        } else { perror("recvmmsg"); return 1; }                       // fatal
    }
    std::printf("mode=%s datagrams=%d send_calls=%d recv_calls=%d empty_recv_calls=%d\n", batch ? "batch" : "one-by-one", got, send_calls, recv_calls, empty);
    close(rx); close(tx);
}
