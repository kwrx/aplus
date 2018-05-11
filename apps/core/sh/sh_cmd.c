#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

#include "sh.h"


int sh_cmd_cd(int argc, char** argv) {
    if(argv[1])
        return chdir(argv[1]);

    return -1;
}

int sh_cmd_exit(int argc, char** argv) {
    if(argv[1])
        exit(atoi(argv[1]));

    exit(0);
}

int sh_cmd_export(int argc, char** argv) {
    if(argc < 2) {
        int i;
        for(i = 0; environ[i]; i++)
            fprintf(stdout, "%s\n", environ[i]);

        return 0;
    }

    if(strchr(argv[1], '='))
        putenv(argv[1]);
    else
        setenv("_", argv[1], 1);

    return 0;
}

int sh_cmd_history(int argc, char** argv) {
    int i = 0;
    list_each(sh_history, h)
        fprintf(stdout, "%-6d %s\n", i++, h);

    return 0;
}

struct sh_command_list sh_commands[] = {
    { "cd", "Change current working directory", sh_cmd_cd },
    { "exit", "Exit the shell", sh_cmd_exit },
    { "export", "Set environment variable", sh_cmd_export },
    { "history", "List command history", sh_cmd_history },
    { NULL, NULL, NULL }
};


