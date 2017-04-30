#pragma once

#define HAVE_LOGIN              0
#define HAVE_LUA                1

#define CLRBUF() memset(buf, 0, sizeof(buf))


typedef struct sh_alias {
    char* key;
    union {
        void* value;
        char* string;
        void (*func)(char**);
    };
    int type;

    struct sh_alias* next;
} sh_alias_t;



#define SH_ALIAS_TYPE_STRING    1
#define SH_ALIAS_TYPE_FUNC      2



char* sh_gethostname(void);
char* sh_login(char* hostname);
void sh_cmdline(char* cmdline);
void sh_alias(char* k, void* v, int t);


extern sh_alias_t* sh_aliases;