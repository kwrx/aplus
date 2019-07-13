#include <stdio.h>

int main(int argc, char** argv, char** envp) {
    write(1, "Hello World", 11);
    for(;;);
}
