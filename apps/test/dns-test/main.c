/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

/*
 * Walks the exact sequence musl's resolver performs, one syscall at a time, so that a failing
 * getaddrinfo() can be pinned on a specific step instead of the whole stack. In order:
 * socket(SOCK_DGRAM), the same with SOCK_CLOEXEC|SOCK_NONBLOCK (which is what res_msend really
 * asks for), bind() to the wildcard, sendto() a hand-built A query, poll() for the answer and
 * recvfrom() to collect it. The connected variant -- connect() then send()/recv() -- is run
 * afterwards because it exercises a different path through lwIP than sendto() does.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


#define TAG "dns-test: "


/*
 * A minimal query for "example.com" IN A, with recursion desired. Hand-built so that the test
 * does not depend on any of the resolver code it is meant to be testing.
 */
static size_t dns_query(uint8_t* buf, uint16_t id) {

    size_t n = 0;

    buf[n++] = id >> 8;
    buf[n++] = id & 0xFF;
    buf[n++] = 0x01; /* RD */
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x01; /* QDCOUNT */
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x00;
    buf[n++] = 0x00;

    buf[n++] = 7;
    memcpy(&buf[n], "example", 7);
    n += 7;
    buf[n++] = 3;
    memcpy(&buf[n], "com", 3);
    n += 3;
    buf[n++] = 0;

    buf[n++] = 0x00;
    buf[n++] = 0x01; /* QTYPE  = A  */
    buf[n++] = 0x00;
    buf[n++] = 0x01; /* QCLASS = IN */

    return n;
}


static void probe(const char* server, int connected) {

    uint8_t query[64];
    uint8_t reply[512];

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(53);
    sa.sin_addr.s_addr = inet_addr(server);


    printf(TAG "--- %s, %s ---\n", server, connected ? "connect()+send()" : "sendto()");


    //? What musl actually asks for. A kernel that rejects the flags makes musl retry without
    //? them, so this is reported rather than fatal.
    int flagged = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);

    if (flagged < 0)
        printf(TAG "socket(SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK) = -1 (%s)\n", strerror(errno));
    else {
        printf(TAG "socket(SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK) = %d\n", flagged);
        close(flagged);
    }


    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        printf(TAG "socket(SOCK_DGRAM) = -1 (%s)\n", strerror(errno));
        return;
    }

    printf(TAG "socket(SOCK_DGRAM) = %d\n", fd);


    struct sockaddr_in any;
    memset(&any, 0, sizeof(any));
    any.sin_family      = AF_INET;
    any.sin_port        = 0;
    any.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr*)&any, sizeof(any)) < 0)
        printf(TAG "bind(0.0.0.0:0) = -1 (%s)\n", strerror(errno));
    else {

        struct sockaddr_in bound;
        socklen_t blen = sizeof(bound);

        if (getsockname(fd, (struct sockaddr*)&bound, &blen) < 0)
            printf(TAG "bind() ok, getsockname() = -1 (%s)\n", strerror(errno));
        else
            printf(TAG "bind() ok, local port %d\n", ntohs(bound.sin_port));
    }


    size_t qlen = dns_query(query, 0x1234);
    ssize_t sent;

    if (connected) {

        if (connect(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
            printf(TAG "connect() = -1 (%s)\n", strerror(errno));
            close(fd);
            return;
        }

        printf(TAG "connect() ok\n");

        sent = send(fd, query, qlen, 0);
        printf(TAG "send(%zu) = %zd (%s)\n", qlen, sent, sent < 0 ? strerror(errno) : "ok");

    } else {

        sent = sendto(fd, query, qlen, MSG_NOSIGNAL, (struct sockaddr*)&sa, sizeof(sa));
        printf(TAG "sendto(%zu) = %zd (%s)\n", qlen, sent, sent < 0 ? strerror(errno) : "ok");
    }


    struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};

    int p = poll(&pfd, 1, 4000);

    printf(TAG "poll(POLLIN, 4000) = %d, revents = 0x%x (%s)\n", p, pfd.revents, p < 0 ? strerror(errno) : "ok");


    if (p > 0) {

        struct sockaddr_in from;
        socklen_t flen = sizeof(from);

        ssize_t got = recvfrom(fd, reply, sizeof(reply), 0, (struct sockaddr*)&from, &flen);

        if (got < 0)
            printf(TAG "recvfrom() = -1 (%s)\n", strerror(errno));
        else
            printf(TAG "recvfrom() = %zd bytes from %s:%d, id 0x%02x%02x, rcode %d, ancount %d\n", got, inet_ntoa(from.sin_addr), ntohs(from.sin_port), reply[0], reply[1], reply[3] & 0x0F,
                   (reply[6] << 8) | reply[7]);
    }


    close(fd);
}


int main(int argc, char** argv) {

    __attribute__((unused)) int unused = argc + (argv != NULL);


    printf(TAG "==== raw UDP probes ====\n");

    probe("10.0.2.3", 0);
    probe("10.0.2.3", 1);
    probe("1.1.1.1", 0);


    printf(TAG "==== resolver ====\n");

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;

    int e = getaddrinfo("example.com", "80", &hints, &res);

    if (e != 0)
        printf(TAG "getaddrinfo(example.com) = %d (%s), errno %d (%s)\n", e, gai_strerror(e), errno, strerror(errno));
    else {

        for (struct addrinfo* p = res; p; p = p->ai_next)
            printf(TAG "getaddrinfo(example.com) -> %s\n", inet_ntoa(((struct sockaddr_in*)p->ai_addr)->sin_addr));

        freeaddrinfo(res);
    }


    //? Same call against a literal, which never reaches DNS: it isolates the resolver from
    //? getaddrinfo() itself.
    res = NULL;
    e   = getaddrinfo("93.184.216.34", "80", &hints, &res);

    printf(TAG "getaddrinfo(93.184.216.34) = %d (%s)\n", e, e ? gai_strerror(e) : "ok");

    if (e == 0)
        freeaddrinfo(res);


    printf(TAG "==== done ====\n");

    return 0;
}
