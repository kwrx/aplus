#ifndef _COREUTILS_H
#define _COREUTILS_H

typedef struct {
    string short_opt;
    string long_opt;
    bool arg;
    
    union {
        void (*with_arg) (string arg);
        void (*without_arg) ();
    } handler;
} argv_t;

static void __args_parse(list<argv_t> x, char** argv) {
    for(list::iterator i = x.begin(); i != x.end(); x++) {
        
    }
}

#define ARG_BEGIN()         \
    list<argv_t> __args
    
 #define ARG(a, b, c, d)    \
        __args.insert(new argv_t() { a, b, c, d })
        
 #define ARG_END(a)          \
    __args_parse(__args, a)
    
#endif