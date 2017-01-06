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



j_void __Z(ZDebug_Print) (char* s) {
    fprintf(stderr, "ZDebug: %s\n", s);
}