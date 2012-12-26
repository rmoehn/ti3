
#include "errors.h"
#include "messages.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#define BACKLOG_SIZE 7

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 3) {
        errx(ARG_ERROR, "Arguments: <IPv6 address> <port number>");
    }

    // Create a socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        err(SOCK_ERROR, "Cannot create socket");
    }

    // Create the address structure
    struct in6_addr address;
    if (inet_pton(AF_INET6, argv[1], &address) != 1) {
        err(ARG_ERROR, "Invalid IPv6 address given");
    }

    // Create the socket address
    struct sockaddr_in6 sock_addr;
    memset(&sock_addr, 0, sizeof(struct sockaddr_in6));
    sock_addr.sin6_port     = htons(atoi(argv[2]));
    sock_addr.sin6_flowinfo = 0;
    sock_addr.sin6_addr     = address;
    sock_addr.sin6_scope_id = 0;

    // Bind the socket to the specified address and port
    if (connect(
            sock_fd,
            (struct sockaddr *) &sock_addr,
            sizeof(struct sockaddr)
        ) == -1) {
        err(SOCK_ERROR, "Cannot connect socket");
    }

    fprintf(stdout, "Here0\n");
    char bla[4] = "bla";
    send(sock_fd, bla, 4, 0);
        err(SEND_ERROR, "Cannot send");
    fprintf(stdout, "Here1\n");
    fprintf(stdout, "Here2\n");

    fprintf(stdout, "Here3\n");
    fflush(stdout);


    // Look what the server sends
    char buffer[MAX_MSG_LEN];
    int recvd_bytes_cnt;
    while ((recvd_bytes_cnt = recv(sock_fd, buffer, MAX_MSG_LEN, 0)) > 0) {
        puts(buffer);
    }
    if (recvd_bytes_cnt == 0) {
        puts("Server closed connection.");
    }
    else {
        err(RECV_ERROR, "Cannot receive from server.");
    }

    return 0;
}
