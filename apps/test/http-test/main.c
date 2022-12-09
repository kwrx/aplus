/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

#if CONFIG_HAVE_NETWORK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>


#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>



int main(int argc, char** argv) {
       
    // GET a web page from a server

    struct sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = htons(80);
    in.sin_addr.s_addr = inet_addr("80.249.99.148");

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock < 0) {
        perror("socket");
        exit(1);
    }

    if(connect(sock, (struct sockaddr*)&in, sizeof(in)) < 0) {
        perror("connect");
        exit(1);
    }


    char* request = "GET /512MB.zip HTTP/1.1\r\nHost: ipv4.download.thinkbroadband.com\r\n\r\n";

    if(send(sock, request, strlen(request), 0) < 0) {
        perror("send");
        exit(1);
    }



    size_t total = 0;
    size_t speed = 0;
    size_t curspeed = 0;
    time_t last = time(NULL);

    do {

        char buffer[1024];
        int n = recv(sock, buffer, sizeof(buffer), 0);

        if(n < 0) {
            perror("recv");
            exit(1);
        }

        if(n == 0) {
            break;
        }

        total += n;
        speed += n;

        if(last != time(NULL)) {
            last = time(NULL);
            curspeed = speed;
            speed = 0;
        }

        fprintf(stderr, "\rReceived: %ld (%ld Kb/s)", total, curspeed / 1024);
        
    } while(1);

    fprintf(stderr, "\nDone!\n");

    close(sock);


    return 0;
    
}

#else

#include <stdio.h>

int main(int argc, char** argv) {
    fprintf(stderr, "Network support not enabled\n");
    return 1;
}

#endif