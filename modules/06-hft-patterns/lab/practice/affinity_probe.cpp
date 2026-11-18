// affinity_probe.cpp - ask the OS to place this thread (Mach affinity tag on macOS, CPU 0 on Linux) and print what it says.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/06-hft-patterns/lab/practice/affinity_probe.cpp -o "${TMPDIR:-/tmp}/affinity_probe" && "${TMPDIR:-/tmp}/affinity_probe"
// Ask the OS to place this thread with an affinity tag, and print what it says.
#include <cstdio>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
int main() {
    thread_affinity_policy_data_t pol = {1};
    kern_return_t kr = thread_policy_set(pthread_mach_thread_np(pthread_self()),
        THREAD_AFFINITY_POLICY, (thread_policy_t)&pol, THREAD_AFFINITY_POLICY_COUNT);
    std::printf("thread_policy_set(THREAD_AFFINITY_POLICY) = %d (%s); KERN_NOT_SUPPORTED = %d\n",
                kr, mach_error_string(kr), KERN_NOT_SUPPORTED);
}
#else
#include <pthread.h>
#include <sched.h>
int main() {
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(0, &set);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof set, &set);
    std::printf("pthread_setaffinity_np(cpu 0) = %d\n", rc);
}
#endif
