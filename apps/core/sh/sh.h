#pragma once

#include <aplus/base.h>
#include <aplus/utils/list.h>

typedef int (*sh_command_t) (int, char**);

struct sh_command_list {
    char* cmd;
    char* description;
    sh_command_t fn;
};


extern list(char*, sh_history);
extern struct sh_command_list sh_commands[];

char* sh_prompt(char* line, char* user, char* host, int last_err);
void sh_history_add(char* line);
int sh_exec(char*);
void sh_reset_tty(void);
