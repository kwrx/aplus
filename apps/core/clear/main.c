#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>



int main(int argc, char** argv) {
    fprintf(stdout, "\e[2J");
    fflush(stdout);
    return 0;
}