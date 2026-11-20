// export_loader.cpp - dlopen each plugin build and report whether dlsym finds public_value and internal_value.
// Linux only. Run from the tutorial root: bash modules/07-architecture/lab/practice/export_check.sh
#include <dlfcn.h>
#include <cstdio>
#include <initializer_list>
int main(){
    for(const char* name:{"./plain.so","./hidden.so","./api.so"}){
        void* h=dlopen(name,RTLD_NOW|RTLD_LOCAL);
        if(!h){std::fprintf(stderr,"%s\n",dlerror());return 1;}
        void* pub=dlsym(h,"public_value");void* priv=dlsym(h,"internal_value");
        std::printf("%s public=%d internal=%d\n",name,pub!=nullptr,priv!=nullptr);
        if(dlclose(h)!=0)return 2;
    }
}
