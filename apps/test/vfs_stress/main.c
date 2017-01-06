#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>

extern int alphasort(const struct dirent** d1, const struct dirent** d2);

int main(int argc, char** argv) {
    
    struct dirent** list;
    int i = scandir("/usr", &list, NULL, alphasort);
    
    if(i < 0)
        perror("/");
    else
        while(i--)
            printf("%s\n", list[i]->d_name);
    
    return 0;
}