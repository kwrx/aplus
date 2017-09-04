#include <aplus/base.h>
#include <aplus/crypto/md5.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Hello World = %s\n", md5("Hello World"));
    return 0;
}
