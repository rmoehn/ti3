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
#include <signal.h>

#define BACKLOG_SIZE 7

void shut_down(int);
FILE *logfile;
int sock_fd;

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 4) {
        errx(ARG_ERROR, "Arguments: <IPv6 address> <port number>"
                        " <path to logfile>");
    }

    // Shut down properly on SIGINT
    struct sigaction shutdown_handler;
    memset(&shutdown_handler, 0, sizeof(struct sigaction));
    shutdown_handler.sa_handler = shut_down;
    if (sigaction(2, &shutdown_handler, NULL) == -1) {
        err(SIG_ERROR, "Cannot install handler for SIGINT");
    }

    // Open the log file
    logfile = fopen(argv[3], "a");
    if (logfile == NULL) {
        err(OUTPUT_ERROR, "Cannot open %s for appending", argv[3]);
    }

    // Create a socket
    sock_fd = socket(AF_INET6, SOCK_STREAM, 0);
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
    sock_addr.sin6_family   = AF_INET6;
    sock_addr.sin6_port     = htons(atoi(argv[2]));
    sock_addr.sin6_flowinfo = 0;
    sock_addr.sin6_addr     = address;
    sock_addr.sin6_scope_id = 0;

    // Bind the socket to the specified address and port
    if (bind(
            sock_fd,
            (struct sockaddr *) &sock_addr,
            sizeof(struct sockaddr_in6)
        ) == -1) {
        err(SOCK_ERROR, "Cannot bind socket");
    }

    // Mark it as a passive port
    if (listen(sock_fd, BACKLOG_SIZE) == -1) {
        err(SOCK_ERROR, "Cannot listen on socket");
    }

    // Accept the next waiting connection
    int client_sock_fd;
    struct sockaddr_in6 client_address;
    socklen_t sockaddrlen = sizeof(struct sockaddr);
    while ((client_sock_fd = accept(
                                 sock_fd,
                                 (struct sockaddr *) &client_address,
                                 &sockaddrlen
                             )) != -1) {
        // Initialise some data
        char addr_string[INET6_ADDRSTRLEN];

        // Log some information about the connection
        fprintf(
            logfile,
            "A: %s\tP: %d\tT: %d\n",
            inet_ntop(
                AF_INET6,
                (void *) &(client_address.sin6_addr),
                addr_string,
                INET6_ADDRSTRLEN
            ),
            ntohs(client_address.sin6_port),
            (int) time(NULL)
        );
        fflush(logfile);

        // Ditch the client
        if (send(client_sock_fd, SERVER_CANCEL, strlen(SERVER_CANCEL) + 1, 0)
            == -1) {
            err(SEND_ERROR, "Cannot send to the client");
        }
        if (close(client_sock_fd) == -1) {
            err(SOCK_ERROR, "Error in closing connection");
        }
    }

    return 0;
}

// Shut the server down properly
void shut_down(int sig_nr)
{
    // Close the logfile
    if (fclose(logfile) == EOF) {
        err(OUTPUT_ERROR, "Problem closing the logfile");
    }

    // Close the socket
    if (close(sock_fd) == -1) {
        err(SOCK_ERROR, "Problem closing server's socket");
    }

    exit(0);
}
