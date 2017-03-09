#ifndef _APLUS_SYSCONFIG_H
#define _APLUS_SYSCONFIG_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aplus/base.h>


/*#define SYSCONFIG_VERBOSE         1*/
#define SYSCONFIG_FORMAT_STRING     's'
#define SYSCONFIG_FORMAT_INT        'i'
#define SYSCONFIG_FORMAT_FLOAT      'f'


static inline uintptr_t sysconfig(const char* option, char fmt, uintptr_t onerr) {
    FILE* fp = fopen(PATH_SYSCONFIG, "r");
    if(!fp) {
#ifdef SYSCONFIG_VERBOSE
        fprintf(stderr, PATH_SYSCONFIG ": file not found!\n");
#endif
        
        errno = ENOENT;
        return onerr;
    }
    
    
    int line_count = 0;
    static char name[BUFSIZ];
    static char value[BUFSIZ];
    
#ifdef __cplusplus
    auto parse_line = [&](char* l) {
#else    
    int parse_line(char* l) {
#endif
        memset(name, 0, sizeof(name));
        memset(value, 0, sizeof(value));
        
        int i, tk_eq, n, v;
        for(i = tk_eq = n = v = 0; l[i]; i++) {
            if(l[i] == '#')
                break;
                
            switch(l[i]) {
                case ' ':
                case '\n':
                case '\r':
                    continue;
                case '=':
                    tk_eq = 1;
                    break;
                default:
                    if(!tk_eq)
                        name[n++] = l[i];
                    else
                        value[v++] = l[i];
            }        
        }
        
        
        line_count++;
        
        if(n == 0 && v == 0)
            return -1;
        
        if(!(n > 0 && v > 0)) {
#ifdef SYSCONFIG_VERBOSE
            fprintf(stderr, PATH_SYSCONFIG ": syntax error at line %d (%d, %d)\n", line_count, n, v);
#endif

            errno = EINVAL;
            return -1;
        }
        
        if(strcmp(name, option) != 0)
            return -1;
        
        return 0;
    };
    
    
    
    fseek(fp, 0, SEEK_SET);
    
    static char line[BUFSIZ];
    while(fgets(line, BUFSIZ, fp)) {
        if(parse_line(line) != -1) {
            fclose(fp);
            
            switch(fmt) {
                case SYSCONFIG_FORMAT_STRING:
                    return (uintptr_t) strdup(value);
                case SYSCONFIG_FORMAT_INT:
                    return (uintptr_t) atoi(value);
                case SYSCONFIG_FORMAT_FLOAT:
                    return (uintptr_t) atof(value);
                default:
                    return onerr;
            }
        }
    }
           
    
    fclose(fp);
    return onerr;
}


#endif