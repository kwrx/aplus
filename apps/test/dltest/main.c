#include <stdio.h>
#include <dlfcn.h>


int main(int argc, char** argv) {
    void* h = dlopen("libdltest.so", RTLD_NOW);
    if(!h) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return -1;
    }

    int (*add) (int, int) = (int (*)(int, int)) dlsym(h, "add");
    if(!add) {
        fprintf(stderr, "dlsym (\"add\"): %s\n", dlerror());
        return -1;
    }

    fprintf(stderr, "add: %p (6 + 5) = %d\n", add, add(6, 5));

    int (*mul) (int, int) = (int (*)(int, int)) dlsym(h, "mul");
    if(!mul)
        fprintf(stderr, "dlsym (\"mul\"): %s\n", dlerror());
    

    fprintf(stderr, "Done!\n");

    dlclose(h);
    return 0;
}