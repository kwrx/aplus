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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
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
       
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(sockfd, 5) == -1) {
        perror("listen");
        return 1;
    }

    do {

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);


        fprintf(stderr, "waiting for connection...\n");

        int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_fd == -1) {
            perror("accept");
            return 1;
        }

        if(fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
            perror("fcntl");
            return 1;
        }

        
        char buf[1024];
        ssize_t e;

        
        do {

            if((e = read(client_fd, buf, sizeof(buf))) < 0) {

                if(errno == EAGAIN)
                    continue;

                perror("read");
                return 1;

            }

            write(1, buf, e);

        } while(e > 0);
        

        fprintf(stderr, "send response...\n");
        
        write(client_fd, "HTTP/1.1 200 OK\r\n\r\n<h1>Hello World from aplus!</h1>", 52);
        close(client_fd);

        fprintf(stderr, "done\n");
        

    } while(1);


    close(sockfd);

    return 0;
    
}