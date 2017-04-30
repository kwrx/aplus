#include <stdio.h>
#include <aplus/base.h>

#define GNX_DYNLIB
#include <aplus/gnx.h>


int main(int argc, char** argv) {
    GnxLoadLibrary(gnx);
    gnx->Initialize();

    
    GnxUnloadLibrary(gnx);
    return 0;
}