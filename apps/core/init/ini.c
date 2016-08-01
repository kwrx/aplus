#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>


char* ini_read(FILE* fp, const char* ini_name) {
    
    int line_count = 0;
    static char name[BUFSIZ];
    static char value[BUFSIZ];
    
    
    int parse_line(char* l) {
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
            fprintf(stderr, "ini_read: syntax error at line %d (%d, %d)\n", line_count, n, v);
            return -1;
        }
        
        if(strcmp(name, ini_name) != 0)
            return -1;
        
        return 0;
    }
    
    
    
    fseek(fp, 0, SEEK_SET);
    
    static char line[BUFSIZ];
    while(fgets(line, BUFSIZ, fp))
        if(parse_line(line) != -1)
            return strdup(value);  
           
           
    return 0;
}


int ini_read_int_or(FILE* fp, const char* ini_name, int onerr) {
    register int r = onerr;
    char* s = ini_read(fp, ini_name);
    if(!s)
        return r;
        
    r = atoi(s);
    free(s);
    
    return r;
}