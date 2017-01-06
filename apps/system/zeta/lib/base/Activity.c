#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <avm.h>

#include "../../zeta.h"



j_void __Z(Activity_Load) (char* name) {
    char buf[BUFSIZ];
    sprintf("%s/%s.app", ZETA_APPS_PATH, name);
    
    
    char* filename = strdup(buf);
    
    if(avm_open(filename) == J_ERR) {
        athrow(NULL, "java/lang/FileNotFoundException", filename);
        return;
    }
    
    avm_call(name, "OnLoad", 0);
}