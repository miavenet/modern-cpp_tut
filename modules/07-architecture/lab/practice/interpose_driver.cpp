// interpose_driver.cpp - dlopen the plugin given on the command line and print answer().
// Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/interpose_check.sh
#include <dlfcn.h>
#include <cstdio>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    void* h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!h){std::fprintf(stderr,"%s\n",dlerror());return 3;}
    auto answer=reinterpret_cast<int(*)()>(dlsym(h,"answer"));
    if(!answer)return 4;
    std::printf("%s answer=%d\n",argv[1],answer());
    return dlclose(h)==0?0:5;
}
