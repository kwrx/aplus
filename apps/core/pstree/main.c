/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <aplus/base.h>
#include <aplus/utils/list.h>

typedef struct p {
    char name[BUFSIZ];
    char path[BUFSIZ];
    char state[16];
    pid_t pid;
    pid_t ppid;
    gid_t tgid;

    list(struct p*, childs);
} p_t;

static void show_usage(int argc, char** argv) {
    printf(
        "Use: pstree [options]...\n"
        "Display a tree of processes.\n\n"
        "   -a, --arguments             show command line arguments\n"
        "   -h, --highlight-all         highlight current process and its ancestors\n"
        "   -g, --show-pgids            show process group ids\n"
        "   -p, --show-pids             show PIDs\n"
        "   -s, --show-parents          show parents of the selected process\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus coreutils) 0.1.%s\n"
        "Copyright (c) %s Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], COMMIT, __DATE__ + 7, __VERSION__, __TIMESTAMP__
    );
  
    exit(0);
}


static int l_min(list(int, pids)) {
    int p = 999999999;
    list_each(pids, v)
        if(v < p)
            p = v;

    if(p == 999999999)
        return -1;

    return p;
}

static p_t* p_find(p_t* root, pid_t pid) {
    if(!root)
        return NULL;

    if(root->pid == pid)
        return root;

    p_t* r;
    list_each(root->childs, p)
        if((r = p_find(p, pid)))
            return r;

    return NULL;
}

static void p_print(p_t* p, size_t depth, int indented, int more, char lines[]) {
    int i;
    for(i = 0; i < strlen(p->name) + 3; i++)
        lines[depth + i] = 0;

    if(!indented && depth) {
        if(more) {
            printf("─┬─");
            lines[depth + 1] = 1;
        } else
            printf("───");

        depth += 3;
    } else if(depth) {
        for(i = 0; i < depth; i++)
            if(lines[i])
                printf("│");
            else
                printf(" ");

        if(more) {
            printf(" ├─");
            lines[depth + 1] = 1;
        } else
            printf(" └─");

        depth += 3;
    }

    printf(p->name);
    if(list_length(p->childs) == 0)
        printf("\n");
    else {
        depth += strlen(p->name);

        int j = 0;
        list_each(p->childs, c)
            p_print(c, depth, !!(j++), ((j + 1) != list_length(p->childs)), lines);
    }

    for(i = 0; i < strlen(p->name) + 3; i++)
        lines[depth + i] = 0;
}


int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "arguments", no_argument, NULL, 'a'},
        { "highlight-all", no_argument, NULL, 'h'},
        { "show-pgids", no_argument, NULL, 'g'},
        { "show-pids", no_argument, NULL, 'p'},
        { "show-parents", no_argument, NULL, 's'},
        { "verbose", no_argument, NULL, 'v'},
        { "help", no_argument, NULL, 'q'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };


    int show_args = 0;
    int show_current = 0;
    int show_pgids = 0;
    int show_pids = 0;
    int show_parents = 0;


    int c, idx;
    while((c = getopt_long(argc, argv, "ahgps", long_options, &idx)) != -1) {
        switch(c) {
            case 'a':
                show_args = 1;
                break;
            case 'h':
                show_current = 1;
                break;
            case 'g':
                show_pgids = 1;
                break;
            case 'p':
                show_pgids = 1;
                break;
            case 's':
                show_pgids = 1;
                break;
            case 'v':
                show_version(argc, argv);
                break;
            case 'q':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }
    

    DIR* d = opendir("/proc");
    if(!d) {
        perror("/proc");
        return -1;
    }


    p_t* root = NULL;
    list(int, pids);

    struct dirent* ent;
    while((ent = readdir(d))) {
        if(!isdigit(ent->d_name[0]))
            continue;

        list_push(pids, atoi(ent->d_name));
    }

    closedir(d);



    while(list_length(pids) > 0) {
        int pid = l_min(pids);
        if(pid == -1)
            break;

        list_remove(pids, pid);


        p_t* p = (p_t*) calloc(sizeof(p_t), 1);
        if(!p) {
            perror("malloc");
            return -1;
        }

        char tmp[BUFSIZ];
        snprintf(tmp, BUFSIZ, "/proc/%d/status", pid);

        FILE* fp = fopen(tmp, "r");
        if(!fp) {
            perror(tmp);
            continue;
        }

        char ln[BUFSIZ];
        while(fgets(ln, BUFSIZ, fp)) {
            if(strstr(ln, "Pid:") == ln)
                sscanf(ln, "%s %d", tmp, &p->pid);
            else if(strstr(ln, "PPid:") == ln)
                sscanf(ln, "%s %d", tmp, &p->ppid);
            else if(strstr(ln, "Tgid:") == ln)
                sscanf(ln, "%s %d", tmp, &p->tgid);
            else if(strstr(ln, "Name:") == ln)
                sscanf(ln, "%s %s", tmp, &p->name);
            else if(strstr(ln, "Path:") == ln)
                sscanf(ln, "%s %s", tmp, &p->path);
            else if(strstr(ln, "State:") == ln)
                sscanf(ln, "%s %s %s", tmp, &p->state, tmp);
        }

        fclose(fp);

        if(strcmp(p->state, "Z") == 0) {
            free(p);
            continue;
        }

        if(p->tgid != p->pid) {
            snprintf(tmp, BUFSIZ, "{%s}", p->name);
            strcpy(p->name, tmp);
        }

        if(p->ppid == 0 && p->pid == 1)
            root = p;
        else {
            p_t* j = p_find(root, p->ppid);
            if(!j)
                continue;

            list_push(j->childs, p);
        }
    }



    char lines[500];
    memset(lines, 0, sizeof(lines));

    p_print(root, 0, 0, 0, lines);
    fflush(stdout);
    return 0;
}