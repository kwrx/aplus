#include <stdio.h>


int main(int argc, char** argv) {
    if(argc > 1) {
        if(*argv[1] == 's')
            abort();
        if(*argv[1] == 'r')
            return 0;
    }
    return 1;
}