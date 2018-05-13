#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>


list(char*, sh_history);

void sh_history_add(char* line) {
    if(strlen(line))
        list_push(sh_history, strdup(line));
}