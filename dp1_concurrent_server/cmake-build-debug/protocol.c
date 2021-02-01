
/*
 *  Library of functions for sockets
 *  Distributed Programming I
 *
 * 	File name: protocol.c
 * 	Programmer: Victor Cappa
 * 	Date last modification: 29/04/2019
 *
 */

#include    <stdlib.h>
#include    <string.h>
#include    <inttypes.h>
#include    <sys/stat.h>
#include    <errno.h>
#include    <syslog.h>
#include    <unistd.h>
#include    <stdio.h>
#include    <bits/types/FILE.h>
#include    <sys/socket.h>
#include    <sys/select.h>
#include    <sys/wait.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <netdb.h>
#include    "protocol.h"


int Select(int max_fd, fd_set *read_set, fd_set *write_set, fd_set *except_set, struct timeval *timeout) {
    int n;
    again:
    if ( (n = select (max_fd, read_set, write_set, except_set, timeout)) < 0)
    {
        if (errno == EINTR)
            goto again;
        else {
            printf("error - select() failed");
            return -1;
        }
    }
    return n;
}

int Socket (int family, int type, int protocol) {
    int n;
    if ( (n = socket(family,type,protocol)) < 0){
        printf("error - socket() failed\n");
        exit(-1);
    }
    return n;
}


void Bind (int sockfd, const struct sockaddr *myaddr,  socklen_t myaddrlen) {
    if ( bind(sockfd, myaddr, myaddrlen) != 0){
        printf("error - bind() failed\n");
        exit(-1);
    }
}


void Listen (int sockfd, int backlog) {
    char *ptr;
    if ( (ptr = getenv("LISTENQ")) != NULL)
        backlog = atoi(ptr);
    if ( listen(sockfd,backlog) < 0 ) {
        printf("error - listen() failed\n");
        exit(-1);
    }
}


int Accept (int listen_sockfd, struct sockaddr *cliaddr, socklen_t *addrlenp) {
    int n;
    again:
    if ( (n = accept(listen_sockfd, cliaddr, addrlenp)) < 0)
    {
        if (errno == EINTR || errno == EPROTO || errno == ECONNABORTED || errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM)
            goto again;
        else {
            printf("error - accept() failed\n");
            return -1;
        }
    }
    return n;
}


void Connect (int sockfd, const struct sockaddr *srvaddr, socklen_t addrlen) {
    if (connect(sockfd, srvaddr, addrlen) != 0) {
        printf("error - connect() failed\n");
        exit(-1);
    }
}


void Close (int fd) {
    if (close(fd) != 0) {
        printf("error - close() failed for socket %d\n", fd);
        //exit(-1); don't exit on Close error, just print error on screen
    }
}


/*
 * n_elements must be lower than maximum size of buffer, timeout for select() is set at 15 seconds
*/
int recv_n (int connected_socket, char* buffer, size_t n_elements) {
    char* buf_cursor = buffer;
    ssize_t new_received;
    size_t to_read = n_elements;

    struct timeval timer;
    fd_set socket_reading;
    int outcome = 0;

    while (to_read > 0) {
        FD_ZERO(&socket_reading);
        FD_SET(connected_socket, &socket_reading);
        timer.tv_sec = 15;
        timer.tv_usec = 0;
        outcome = Select(FD_SETSIZE, &socket_reading, NULL, NULL, &timer);
        if (outcome == 0) {
            /* timeout expired */
            return -2;
        }
        else if (outcome < 0) {
            /* error happened*/
            return -1;
        }
        else {
            new_received = recv(connected_socket, buf_cursor, to_read, 0);
            if (new_received <= 0) {
                return -1;
            }

            to_read -= new_received;
            buf_cursor += new_received;
        }
    }

    return 1;
}


/*
 * n_elements must be lower than maximum size of buffer
 */
int send_n(int connected_socket, const char* buffer, size_t n_elements) {
    char* buffer_cursor = (char* )buffer;
    ssize_t new_sent;
    size_t to_write = n_elements;

    while (to_write > 0) {

        new_sent = send(connected_socket, buffer_cursor, to_write, MSG_NOSIGNAL);
        if (new_sent <= 0) {
            if (errno == EINTR) {
                new_sent = 0;
                continue;
            }
            return -1;
        }

        buffer_cursor += new_sent;
        to_write -= new_sent;
    }

    return 1;
}