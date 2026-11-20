// delete_selection.cpp - which class-specific operator delete does 'delete new T' call: sized only, or sized plus unsized?
// Build and run from the tutorial root; the program goes to your temp directory:
//   clang++ -std=c++20 -O2 -Wall -Wextra modules/05-memory-perf/lab/practice/delete_selection.cpp -o "${TMPDIR:-/tmp}/delete_selection" && "${TMPDIR:-/tmp}/delete_selection"
// Measures selection and bytes passed, not allocator speed.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
void* allocate(std::size_t n){if(void* p=std::malloc(n))return p;throw std::bad_alloc();}
struct SizedOnly {
    std::uint64_t words[3]{};
    static void* operator new(std::size_t n){return allocate(n);}
    static void operator delete(void* p,std::size_t n) noexcept {
        std::printf("only: sized bytes=%zu\n",n);std::free(p);
    }
};
struct Both {
    std::uint64_t words[3]{};
    static void* operator new(std::size_t n){return allocate(n);}
    static void operator delete(void* p) noexcept {std::puts("both: unsized");std::free(p);}
    static void operator delete(void* p,std::size_t n) noexcept {
        std::printf("both: sized bytes=%zu\n",n);std::free(p);
    }
};
int main(){
    std::printf("sizes=%zu,%zu\n",sizeof(SizedOnly),sizeof(Both));
    delete new SizedOnly;
    delete new Both;
}
