
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

#define CLIENTBUFLEN	4096
char *program_name;


/*
 * the function returns:
 * 0 the requested file does not exist on the server
 * 1 successful file transmission
 * -1 error occurred during file transmission
 * -2 select() timeout expired
 *
 * the calling function is responsible of closing the file descriptor.
*/
int receive_file(int connected_socket, char* buf, FILE* write_file, uint32_t* timestamp, uint32_t *file_size) {
    int outcome = 0;

    outcome = recv_n(connected_socket, buf, 1);
    if (outcome < 0) {
        /* error while receiving data from the server, -1 generic error, -2 timeout expired */
        return outcome;
    }

    if (buf[0] == '+') {
        /*  receive and write the file content */
        uint32_t num_bytes = 0;
        outcome = recv_n(connected_socket, buf, 8);
        if (outcome < 0) {
            /* error while receiving data from the server, -1 generic error, -2 timeout expired */
            return outcome;
        }

        if (buf[0] != 'O' || buf[1] != 'K' || buf[2] != '\r' || buf[3] != '\n') {
            /* invalid file transfer */
            return -1;
        }

        char* cursor = (char*) &num_bytes;
        memcpy(&cursor[0], &buf[4], 4);
        num_bytes = ntohl(num_bytes);
        *file_size = num_bytes;

        /* receive the file from the server*/
        int num_blocks = (int)(num_bytes / CLIENTBUFLEN);
        for (int a = 0; a < num_blocks; a++) {
            outcome = recv_n(connected_socket, buf, CLIENTBUFLEN);
            if (outcome < 0) {
                /* error while receiving data from the server, -1 generic error, -2 timeout expired */
                return outcome;
            }
            size_t eff_written = fwrite(&buf[0], sizeof(char), CLIENTBUFLEN, write_file);
            if (eff_written != CLIENTBUFLEN) {
                /* error occurred while writing on the file */
                return -1;
            }
        }
        num_bytes -= (num_blocks * CLIENTBUFLEN);

        if (num_bytes != 0) {
            outcome = recv_n(connected_socket, buf, num_bytes);
            if (outcome < 0) {
                /* error while receiving data from the server, -1 generic error, -2 timeout expired */
                return outcome;
            }
            size_t eff_written = fwrite(&buf[0], sizeof(char), num_bytes, write_file);
            if (eff_written != num_bytes) {
                /* error occurred while writing on the file */
                return -1;
            }
        }
    }
    else if (buf[0] == '-') {
        /*  requested file doesn't exist in target server. Clear the buffer */
        outcome = recv_n(connected_socket, buf, 5);
        if (outcome < 0) {
            /* error while receiving data from the server, -1 generic error, -2 timeout expired */
            return outcome;
        }
        if (buf[0] != 'E' || buf[1] != 'R' || buf[2] != 'R' || buf[3] != '\r' || buf[4] != '\n') {
            /* invalid error message */
            return -1;
        }

        return 0;   /* file doesn't exist in the server */
    }
    else {
        /* illicit response  */
        return -1;
    }

    /* get timestamp of file */
    uint32_t file_time = 0;
    char* cursor = (char*) &file_time;
    outcome = recv_n(connected_socket, buf, 4);
    if (outcome < 0) {
        /* error while receiving data from the server, -1 generic error, -2 timeout expired */
        return outcome;
    }
    memcpy(&cursor[0], &buf[0], 4);
    file_time = ntohl(file_time);
    *timestamp = file_time;

    return 1;
}


int send_request (int connected_socket, char* buffer, const char* file_name) {
    int cursor = 0;

    buffer[cursor++] = 'G';
    buffer[cursor++] = 'E';
    buffer[cursor++] = 'T';
    buffer[cursor++] = ' ';
    for (int i = 0; file_name[i] != '\0'; ++i) {
        buffer[cursor++] = file_name[i];

        if (cursor >= (CLIENTBUFLEN - 2)) {
            return -1;
        }
    }
    buffer[cursor++] = '\r';
    buffer[cursor++] = '\n';
    int outcome = send_n(connected_socket, buffer, (size_t)cursor);
    if (outcome == -1) {
        return -1;
    }

    return 1;
}


int client_service (int connected_socket, char** file_names, int num_requested_files) {
    char buf[CLIENTBUFLEN + 1];
    buf[CLIENTBUFLEN] = '\0';
    int outcome = 0;

    for (int a = 0; a < num_requested_files; a++) {
        /* create file descriptor for file to transfer on local file system */
        FILE* transfer_file = fopen(file_names[a], "w");
        if (transfer_file == NULL) {
            printf("error occurred while opening/creating new file on local file system - closing connection with the server.\n");
            return -1;
        }

        /* send request to Server */
        printf("sending request for file number %d: %s\n", a + 1, file_names[a]);
        outcome = send_request(connected_socket, buf, file_names[a]);
        if (outcome < 0) {
            /* error while sending request to Server */
            printf("error while sending request to server.\n");
            fclose(transfer_file);
            return -1;
        }

        /* receive server response */
        uint32_t timestamp = 0;
        uint32_t file_size = 0;
        outcome = receive_file(connected_socket, buf, transfer_file, &timestamp, &file_size);
        if (outcome == 1) {
            /* successful transfer from server, continue loop */
            printf("Successful file transfer:\n\tname of file: %s\n\tsize of file: %lu\n\ttimestamp of last modification: %lu\n", file_names[a], (unsigned long)file_size, (unsigned long)timestamp);
        }
        else if (outcome == 0) {
            /* requested file does not exist on the server, continue loop */
            printf("requested file doesn't exist in the server.\n");
            fclose(transfer_file);
            remove(file_names[a]);
            return -1;
        }
        else if (outcome == -1) {
            /* error while receiving server's response */
            printf("error during file transmission from server.\n");
            fclose(transfer_file);
            /* removing wrong (not complete) file from local file system */
            remove(file_names[a]);
            return -1;
        }
        else if (outcome == -2) {
            /* timeout of select() expired */
            printf("error occurred - timeout of select() expired during file transfer (15 seconds).\n");
            fclose(transfer_file);
            /* removing wrong (not complete) file from local file system */
            remove(file_names[a]);
            return -1;
        }

        fclose(transfer_file);
        /*  continue with next file request */
    }

    return 1;
}


int main(int argc, char *argv[])
{
    uint16_t tport_n, tport_h;	/* server port number (net/host ord) */
    int connected_socket;
    int outcome;
    struct sockaddr_in	saddr;		/* server address structure */
    struct in_addr	sIPaddr; 	/* server IP addr. structure */

    program_name = argv[0];

    if (argc < 4) {
        printf("Usage: %s <IP server address> <port number> <file name 1> <file name 2> ... <file name n>\n", program_name);
        exit(-1);
    }
    outcome = inet_aton(argv[1], &sIPaddr);
    if (!outcome) {
        printf("error - enter a valid IPv4 address in the command line.\n");
        exit(-1);
    }

    unsigned long tmp_port = strtoul(argv[2], NULL, 0);
    if ((tmp_port == ULONG_MAX) || (tmp_port < 1024) || (tmp_port > 65535)) {
        printf("Enter a valid port number - port numbers must be between 1024 and 65535\n");
        exit(1);
    }
    tport_h = (uint16_t) tmp_port;
    tport_n = htons(tport_h);


    /* create the socket */
    connected_socket = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* prepare address structure server address */
    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port   = tport_n;
    saddr.sin_addr   = sIPaddr;

    /* connect to server*/
    Connect(connected_socket, (struct sockaddr *) &saddr, sizeof(saddr));

    /* get service from the server*/
    client_service(connected_socket, &argv[3], argc - 3);
    printf("End of service for the Client - closing connection with the server.\n");
    Close(connected_socket);

    return 1;
}