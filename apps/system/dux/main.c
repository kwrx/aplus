#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>

#include "lib/duktape.h"
#include "dux.h"

static void show_usage(int argc, char** argv) {
    printf(
        "Use: xjs [OPTIONS...] [FILE]\n"
        "Run JavaScript files\n\n"
        "       --help                  show this help\n"
        "       --version               print version info and exit\n"
    );
    
    exit(0);
}

static void show_version(int argc, char** argv) {
    printf(
        "%s (aPlus sysutils) 0.1\n"
        "Copyright (c) 2016 Antonino Natale.\n"
        "Built with gcc %s (%s)\n",
        
        argv[0], __VERSION__, __TIMESTAMP__
    );
    
    exit(0);
}




int main(int argc, char** argv) {
    
    static struct option long_options[] = {
        { "help", no_argument, NULL, 'h'},
        { "version", no_argument, NULL, 'v'},
        { NULL, 0, NULL, 0 }
    };
    
    
    int c, idx;
    while((c = getopt_long(argc, argv, "", long_options, &idx)) != -1) {
        switch(c) {
            case 'v':
                show_version(argc, argv);
                break;
            case 'h':
            case '?':
                show_usage(argc, argv);
                break;
            default:
                abort();
        }
    }
    

    FILE* fp = stdin;
    char* input = "<stdin>";
    
    if(optind < argc) {
        input = argv[optind];
        fp = fopen(argv[optind], "r");
    }

    
    if(!fp) {
        perror(input);
        return -1;
    }


    fseek(fp, 0, SEEK_END);
    fpos_t p = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buf = (char*) calloc(1, p);
    if(!buf) {
        perror("malloc");
        fclose(fp);
        return -1;
    }


    if(fread(buf, 1, p, fp) != p) {
        perror("I/O");
        fclose(fp);
        return -1;
    }

    fclose(fp);




    duk_context* ctx = duk_create_heap_default();
    
    int i;
    for(i = 0; c_funcs[i].value; i++) {
        duk_push_c_function(ctx, c_funcs[i].value, c_funcs[i].nargs);
        duk_put_global_string(ctx, c_funcs[i].key);
    }


    if(duk_peval_string_noresult(ctx, buf) != 0) {
        fprintf(stderr, "%s: %s\n", input, duk_safe_to_string(ctx, -1));
        return -1;
    }
    
    duk_destroy_heap(ctx);
    return 0;       
}