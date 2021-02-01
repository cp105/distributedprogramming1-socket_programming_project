
/*
 *  SEQUENTIAL TCP SERVER
 *  Distributed Programming I
 *  Exercise2.3 Iterative File Transfer TCP Server
 *
 * 	File name: server1_main.c
 * 	Programmer: Victor Cappa
 * 	Date last modification: 15/05/2019
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
#include    <limits.h>
#include    "protocol.h"


#define SERVERBUFLEN		4096
#define MAX_LEN_FILE_NAME 200
char *program_name;


int get_request(int connected_socket, char* buffer, char* file_name) {

    ssize_t new_received;
    size_t to_read = SERVERBUFLEN;
    char* buf_cursor = buffer;

    struct timeval timer;
    fd_set socket_reading;
    int outcome = 0;

    /*  get the request from the client */
    while (1)
    {
        FD_ZERO(&socket_reading);
        FD_SET(connected_socket, &socket_reading);
        timer.tv_sec = 15;
        timer.tv_usec = 0;
        outcome = Select(FD_SETSIZE, &socket_reading, NULL, NULL, &timer);
        if (outcome <= 0) {
            /* timeout expired or error happened */
            return -1;
        }
        else {
            new_received = recv(connected_socket, buf_cursor, to_read, 0);
            if (new_received <= 0) {
                return -1;
            }

            if(buf_cursor[new_received - 2] == '\r' && buf_cursor[new_received - 1] == '\n') {
                /* termination of reading request is reached */
                break;
            }

            /* continue with the reading loop */
            buf_cursor += new_received;
            to_read -= new_received;

            if(to_read <= 0) {
                /* request message is longer than SERVERBUFLEN bytes and thus is illegal */
                printf("Waiting for a request from client but received an invalid request.\n");
                return -1;
            }
        }

    }

    /*  extrapolate the file name  */
    int count = 0;
    if(buffer[0] == 'G' && buffer[1] == 'E' && buffer[2] == 'T' && buffer[3] == ' ') {
        for(int i = 4; buffer[i] != '\r'; ++i) {
            file_name[count++]  = buffer[i];

            if (count == (MAX_LEN_FILE_NAME - 1))
                break;
        }

        file_name[count] = '\0';
        return count;
    }
    else {
        /* invalid request to server */
        printf("Waiting for a request from client but received an invalid request.\n");
        return -1;
    }
}


int get_file_timestamp (const char* file_name, uint32_t* timestamp, uint32_t* file_size) {
    struct stat my_stat;
    char file_path[100];
    uint32_t tmp = 0, size = 0;

    if (getcwd(file_path, sizeof(file_path)) == NULL) {
        return -1;
    }
    if (strcat(file_path, "/") == NULL) {
        return -1;
    }
    if (strcat(file_path, file_name) == NULL) {
        return -1;
    }
    if(stat(file_path, &my_stat) == -1) {
        return -1;
    }
    tmp = (uint32_t) my_stat.st_mtime;
    *timestamp = tmp;
    size = (uint32_t) my_stat.st_size;
    *file_size = size;

    return 1;
}


int send_file(int connected_socket, char* buffer, FILE* fd_file, uint32_t timestamp_file, uint32_t file_size) {
    int outcome = 0;

    /* send heading of file transfer */
    uint32_t n_characters_net = htonl(file_size);
    char* cursor = (char* ) &n_characters_net;
    buffer[0] = '+';
    buffer[1] = 'O';
    buffer[2] = 'K';
    buffer[3] = '\r';
    buffer[4] = '\n';
    memcpy(&buffer[5], &cursor[0], 4);
    outcome = send_n(connected_socket, buffer, 9);
    if(outcome <= 0) {
        return -1;
    }

    int iterations = (int) (file_size / SERVERBUFLEN);
    for (int a = 0; a < iterations; ++a) {
        size_t eff_read = fread(buffer, sizeof(char), SERVERBUFLEN, fd_file);
        if (eff_read != SERVERBUFLEN) {
            /* error while reading  the file on the file system */
            return -1;
        }
        outcome = send_n(connected_socket, buffer, SERVERBUFLEN);
        if (outcome <= 0) {
            /* error while sending the file */
            return -1;
        }
    }
    if ((file_size % SERVERBUFLEN) != 0) {
        size_t eff_read = fread(buffer, sizeof(char), file_size % SERVERBUFLEN, fd_file);
        if (eff_read != (file_size % SERVERBUFLEN)) {
            /* error while reading  the file on the file system */
            return -1;
        }
        outcome = send_n(connected_socket, buffer, file_size % SERVERBUFLEN);
        if (outcome <= 0) {
            /* error while sending the file */
            return -1;
        }
    }

    /* send timestamp of last file modification */
    uint32_t timestamp_file_net = htonl(timestamp_file);
    cursor = (char* ) &timestamp_file_net;
    memcpy(&buffer[0], &cursor[0], 4);
    outcome = send_n(connected_socket, buffer, 4);
    if (outcome <= 0) {
        /* error while sending the file */
        return -1;
    }

    return 1;	/* success in sending the file */
}


/* returned -1 in case of error */
int send_error_message(int connected_socket) {
    const char error_message[] = "-ERR\r\n";
    size_t error_message_len = 6;

    int outcome = send_n(connected_socket, (const char* )error_message, error_message_len);
    return outcome;
}


int service_server (int connected_socket) {
    /* serve the client on socket s */
    char file_name[MAX_LEN_FILE_NAME + 1];
    file_name[MAX_LEN_FILE_NAME] = '\0';
    char buffer[SERVERBUFLEN + 1];
    buffer[SERVERBUFLEN] = '\0';
    uint32_t file_size = 0;
    int outcome = 0;

    while(1) {
        /* receive request from client */
        int file_name_len = get_request(connected_socket, buffer, file_name);
        if(file_name_len < 0) {
            /* error while getting the request message or end or file requests from Client */
            return -1;
        }

        /* check existence of the file in the working directory of the local file system */
        printf("requested file: %s\n", file_name);
        FILE* my_file = fopen(file_name, "r");
        if(my_file == NULL) {
            /* requested file does not exit on the server, send error message to client, end of service for the Client */
            printf("requested file does not exist on the server\n");
            send_error_message(connected_socket);
            return -1;
        }
        else {
            /* file does exist on the server, get last timestamp of file and its size, send file */
            uint32_t timestamp;
            outcome = get_file_timestamp(file_name, &timestamp, &file_size);
            if (outcome < 0) {
                /* end of service for the Client */
                printf("error while getting timestamp and size for file %s\n", file_name);
                fclose(my_file);
                return -1;
            }
            /* send request response to client (send file) */
            outcome = send_file(connected_socket, buffer, my_file, htonl(timestamp), file_size);
            if (outcome < 0) {
                /* error while sending the file to the Client, end of service for the Client */
                printf("error occurred while sending file to client\n");
                fclose(my_file);
                return -1;    /* exit and start listening (accept) for a new client */
            }

            fclose(my_file);
        }

        printf("file transfer was successful.\n");
        /* successful delivery of file to Client, continue waiting for a new request from the same Client */
    }
}


int main(int argc, char *argv[])
{
    int		passive_socket;	/* passive socket */
    uint16_t 	lport_n, lport_h;	/* port used by server (net/host ord.) */
    struct sockaddr_in 	saddr, caddr;	/* server and client addresses */

    program_name = argv[0];

    if (argc != 2) {
        printf("Usage: %s <port number>\n", program_name);
        exit(1);
    }

    unsigned long tmp_port = strtoul(argv[1], NULL, 0);
    if ((tmp_port == ULONG_MAX) || (tmp_port < 1024) || (tmp_port > 65535)) {
        printf("Enter a valid port number - port numbers must be between 1024 and 65535\n");
        exit(1);
    }
    lport_h = (uint16_t) tmp_port;
    lport_n = htons(lport_h);

    /* create the socket */
    passive_socket = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* bind the socket to any local IP address */
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family      = AF_INET;
    saddr.sin_port        = lport_n;
    saddr.sin_addr.s_addr = INADDR_ANY;
    Bind(passive_socket, (struct sockaddr *) &saddr, sizeof(saddr));

    /* listen */
    int		bk_log = 5;                                             /* listen backlog */
    Listen(passive_socket, bk_log);

    /* main server loop */
    int	 	s;			                                /* current connected socket (SEQUENTIAL SERVER) */
    socklen_t addr_len = sizeof(struct sockaddr_in);
    printf("Waiting for first Client connection...\n");

    while (1)
    {
        /* accept next connection */
        s = Accept(passive_socket, (struct sockaddr *) &caddr, &addr_len);
        if (s < 0) {
            /* start listening to a new connection */
            continue;
        }

        printf("Accepted new connection on socket %d.\n", s);
        service_server(s);
        printf("End of service for the client on socket %d - closing the connection.\n", s);
        Close(s);
    }
}