#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <aplus/base.h>
#include <aplus/utils/unittest.h>



static int lookup(char* url) {
     struct hostent* e;

    fprintf(stdout, "Try \'%s\' => ", url);
    e = gethostbyname(url);
    if(!e) {
        fprintf(stdout, "FAILED\n");
        return -1;
    }


    fprintf(stdout, "%d.%d.%d.%d\n", 
            e->h_addr_list[0][0] & 0xFF,
            e->h_addr_list[0][1] & 0xFF,
            e->h_addr_list[0][2] & 0xFF,
            e->h_addr_list[0][3] & 0xFF);

    return 0;
}


int main(int argc, char** argv) {
    __unittest_begin();
    __unittest(lookup("localhost"), ==, 0, int);
    __unittest(lookup("www.google.it"), ==, 0, int);
    __unittest(lookup("www.facebook.com"), ==, 0, int);
    __unittest(lookup("www.osdev.org"), ==, 0, int);
    __unittest(lookup("www.geekstribe.altervista.org"), ==, 0, int);
    __unittest_end();

    return 0;
}