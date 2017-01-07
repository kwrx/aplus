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

 

void* __Z(Activity_Load) (char* name) {
    fprintf(stderr, "Activity.Load(%s : %p)\n", name, name);
    
    char buf[BUFSIZ];
    sprintf(buf, "%s/%s.app", ZETA_APPS_PATH, name);
    
    
    char* filename = strdup(buf);
    
    if(avm_open(filename) == J_ERR)
        athrow(NULL, "java/lang/FileNotFoundException", filename);
    
    
    j_value params[1];
    if(java_object_new((java_object_t**) &params[0], name) == J_ERR)
        athrow(NULL, "java/lang/OutOfMemoryError", "");
    
    java_method_t* method;
    if(java_method_find(&method, name, "OnStart", "()V") == J_ERR)
        athrow(NULL, "java/lang/NoSuchMethodError", "void OnStart()");
    
    java_method_invoke(NULL, method->assembly, method, params, 1);
    return (void*) params[0].ptr;
}