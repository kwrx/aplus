#include <aplus/base.h>
#include <aplus/sysconfig.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>


static sysvalue_t parse_string(char* value) {
    if(value[strlen(value) - 1] == '\"')
        value[strlen(value) - 1] = '\0';

    return (sysvalue_t) &value[1];
}

static sysvalue_t parse_number(char* value) {
    long sign = 1;

    if(value[0] == '+')
        value++;

    if(value[0] == '-') {
        sign = -1;
        value++;
    }

    if(strchr(value, '.')) {
        /* Double */

        double n;
        sscanf(value, "%f", &n);
        return (sysvalue_t) ((long) (n * (double) sign));
    } else {
        /* Int */
        long n;

        if(value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
            sscanf(value, "%x", &n);
        else if(value[0] == '0' && isdigit(value[1]))
            sscanf(value, "%o", &n);
        else
            sscanf(value, "%ld", &n);
        
        return (sysvalue_t) (n * sign);
    }

    return 0;
} 

sysvalue_t __sysconfig(const char* option, sysvalue_t defvalue) {
    FILE* fp = fopen(PATH_SYSCONFIG, "r");
    if(!fp) {
        errno = ENOENT;
        return defvalue;
    }

    fseek(fp, 0, SEEK_SET);



    int pl(char* ln, const char* option, char* value) {
        char name[BUFSIZ];
        memset(name, 0, BUFSIZ);


        int i, tk, n, v;
        for(i = tk = n = v = 0; ln[i]; i++) {
            if(ln[i] == '#')
                break;

            switch(ln[i]) {
                case '\r':
                case '\n':
                    continue;
                case '=':
                    tk++;
                    break;
                default:
                    if(!tk)
                        name[n++] = ln[i];
                    else
                        value[v++] = ln[i];
                    break;
            }
        }

        if(n == 0 && v == 0)
            return -1;

        if(!(n > 0 && v > 0)) {
            errno = EINVAL;
            return -2;
        }

        if(strcmp(name, option) != 0)
            return -1;

        return 0;
    }


    char ln[BUFSIZ];
    while(fgets(ln, BUFSIZ, fp)) {

        char value[256];
        memset(value, 0, sizeof(value));

        if(pl(ln, option, value) < 0)
            continue;


        /* VAR */
        if(value[0] == '$')
            return __sysconfig(&value[1], defvalue);

        /* STRING */
        if(value[0] == '\"')
            return strdup(parse_string(value));

        return parse_number(value);
    }
}