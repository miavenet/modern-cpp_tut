// reuse_inline.cpp - the single-file control: new/delete inline, so the optimizer may delete them.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   clang++ -std=c++20 -O2 -S modules/07-architecture/lab/practice/reuse_inline.cpp -o - | grep -cE '(bl|call|callq)[[:space:]]+_?_Z(nw|dl)'   (then g++ the same way)
// Does a freed 16-byte block come straight back from the next new of the same size?
#include <cstdio>
struct Node { int value; Node* next; };
int main() {
    int same = 0;
    for (int i = 0; i < 1000; ++i) {
        Node* p = new Node{i, nullptr};
        void* addr = p;
        delete p;
        Node* q = new Node{i, nullptr};
        same += (static_cast<void*>(q) == addr);
        delete q;
    }
    std::printf("immediate reuse in %d of 1000 tries\n", same);
}
