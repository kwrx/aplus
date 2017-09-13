#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define htons(x)  __builtin_bswap16(x)
#define htonl(x)  __builtin_bswap32(x)


static void lookup(char* url) {
     struct hostent* e;

    fprintf(stdout, "Try \'%s\' => ", url);
    e = gethostbyname(url);
    if(!e) {
        fprintf(stdout, "FAILED\n");
        return;
    }


    fprintf(stdout, "%d.%d.%d.%d\n", 
            e->h_addr_list[0][0] & 0xFF,
            e->h_addr_list[0][1] & 0xFF,
            e->h_addr_list[0][2] & 0xFF,
            e->h_addr_list[0][3] & 0xFF);
}


int main(int argc, char** argv) {
   lookup("localhost");
   lookup("www.google.it");
   lookup("www.facebook.com");
   lookup("www.osdev.org");
   lookup("www.geekstribe.altervista.org");


    return 0;
}