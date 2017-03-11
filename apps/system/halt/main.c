#include <stdio.h>
#include <unistd.h>



int main(int argc, char** argv) {
    return execlp("shutdown", "shutdown", "--halt", "now", NULL);
}