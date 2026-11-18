// clamp_reader.cpp - one thread clamps (the if form, or with an argument the always-store form) while another thread reads the array.
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -g -fsanitize=thread modules/05-memory-perf/lab/practice/clamp_reader.cpp modules/05-memory-perf/lab/practice/clamp.cpp -o "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" && "${TMPDIR:-/tmp}/clamp_reader" always
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
void clamp32_if(int*,decltype(sizeof 0));
void clamp32_always(int*,decltype(sizeof 0));
int main(int argc,char**) {
 std::vector<int> a(1024,1);
 std::atomic<bool> ready{false};
 std::uint64_t sum=0;
 auto fn=argc>1?clamp32_always:clamp32_if;
 std::thread reader([&]{
   while(!ready.load(std::memory_order_acquire)){}
   for(int r=0;r<10000;++r) for(int x:a)sum+=static_cast<unsigned>(x);
 });
 ready.store(true,std::memory_order_release);
 for(int r=0;r<10000;++r)fn(a.data(),a.size());
 reader.join();
 std::printf("checksum=%llu\n",(unsigned long long)sum);
}
