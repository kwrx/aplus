#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

static int __h_errno = 0;

int* __h_errno_location(void) {
    return &__h_errno;
}


const char* hstrerror(int herrno) {
    static char* errors[] = {
        "Resolver internal error, see errno",
        "Success",
        "Authoritative answer: host not found",
        "Non-authoritative \"host not found\", or SERVERFAIL",
        "Non recoverable errors, FORMERR, REFUSED, NOTIMP",
        "Valid name, no data record of requested type",
        NULL
    };
    
    if(herrno > 4)
        return "Unknown resolver error";
    
    return errors[herrno + 1];
}


void herror(const char* s) {
    if(s)
        fprintf(stderr, "%s: %s\n", s, hstrerror(__h_errno));
    else
        fprintf(stderr, "%s\n", hstrerror(__h_errno));
}
