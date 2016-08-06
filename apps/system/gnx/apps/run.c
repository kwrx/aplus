#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "../gnx.h"


int gnx_apps_run(const char* path) {
    char buf[BUFSIZ];
    sprintf(buf, "%s/config", path);
    
    struct stat st;
    if(stat(buf, &st) != 0) {
        fprintf(stderr, "gnx: %s: %s\n", buf, strerror(errno));
        return -1;
    }
    
    
    return 0;
}