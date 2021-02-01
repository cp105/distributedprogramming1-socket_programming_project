
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int Select(int max_fd, fd_set *read_set, fd_set *write_set, fd_set *except_set, struct timeval *timeout);
int Socket (int family, int type, int protocol);
void Close (int fd);
void Bind (int sockfd, const struct sockaddr *myaddr,  socklen_t myaddrlen);
void Listen (int sockfd, int backlog);
int Accept (int listen_sockfd, struct sockaddr *cliaddr, socklen_t *addrlenp);
void Connect (int sockfd, const struct sockaddr *srvaddr, socklen_t addrlen);
int recv_n (int connected_socket, char* buffer, size_t n_elements);
int send_n(int connected_socket, const char* buffer, size_t n_elements);

#endif