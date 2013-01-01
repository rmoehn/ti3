#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "errors.h"

#define BACKLOG_SIZE 7
#define MAX_CLIENT_CNT 50
#define MAX(a, b) ((a) > (b) ? (a) : (b))

volatile sig_atomic_t got_SIGINT = 0;
void handle_SIGINT(int sig_num)
{
    printf("Handling signal\n");
    got_SIGINT = 1;
}

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 3) {
        errx(ERR_ARG, "Arguments: <IPv6 address> <port number>");
    }

    // Block SIGINT
    sigset_t sigmask;
    sigemptyset( &sigmask         );
    sigaddset(   &sigmask, SIGINT );
    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        err(ERR_SIGNAL, "Cannot block SIGINT");
    }

    // Create empty mask (no blocking) for upcoming pselect
    sigset_t no_block_sigmask;
    sigemptyset(&no_block_sigmask);

    // Install a handler for SIGINT
    struct sigaction SIGINT_action;
    SIGINT_action.sa_handler = handle_SIGINT;
    SIGINT_action.sa_flags   = 0;
    sigfillset( &SIGINT_action.sa_mask );
    if (sigaction(SIGINT, &SIGINT_action, NULL) == -1) {
        err(ERR_SIGNAL, "Cannot install handler for SIGINT");
    }

    // Create a socket
    int sock_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        err(ERR_SOCKET, "Cannot create socket");
    }

    // Create the address structure
    struct in6_addr address;
    if (inet_pton(AF_INET6, argv[1], &address) != 1) {
        err(ERR_ARG, "Invalid IPv6 address given");
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
        err(ERR_SOCKET, "Cannot bind socket");
    }

    // Mark it as a passive port
    if (listen(sock_fd, BACKLOG_SIZE) == -1) {
        err(ERR_SOCKET, "Cannot listen on socket");
    }

    // Set up the socket controls
    int max_fd = sock_fd;
    int client_fds[MAX_CLIENT_CNT];
    int client_cnt = 0;

    // React to events on the watched sockets
    while (1) {
        // Watch the socket to accept calls on
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);

        // Put the current clients under surveillance
        for (int i = 0; i < client_cnt; ++i) {
            FD_SET(client_fds[i], &readfds);
        }

        // Look what has happened
        int pselect_ret = pselect(
                              max_fd + 1,
                              &readfds,
                              NULL, NULL, NULL,
                              &no_block_sigmask
                          );
        if (pselect_ret == -1 && errno != EINTR) {
            err(ERR_SELECT, "Error during select");
        }

        // Shut down gracefully on SIGINT
        if (got_SIGINT) {
            warnx("Caught SIGINT. Shutting down. ");
            int is_proper_shutdown = 1;

            // Close client sockets
            for (int i = 0; i < client_cnt; ++i) {
                if (close(client_fds[i]) == -1) {
                    warn("Problem closing client socket");
                    is_proper_shutdown = 0;
                }
            }

            // Close main socket
            if (close(sock_fd) == -1) {
                warn("Problem closing main socket");
                is_proper_shutdown = 0;
            }

            // Depending on whether all sockets were closed properly, exit
            if (is_proper_shutdown) {
                exit(EXIT_OK);
            }
            else {
                exit(ERR_SHUTDOWN);
            }
        }

        // If there is a new client and there aren't too many clients yet
        if (FD_ISSET(sock_fd, &readfds) && client_cnt < MAX_CLIENT_CNT) {
            // Accept the connection
            struct sockaddr_in6 client_address;
            socklen_t sockaddrlen = sizeof(struct sockaddr);
            int client_sock_fd = accept(
                                     sock_fd,
                                     (struct sockaddr *) &client_address,
                                     &sockaddrlen
                                 );

            // Add the socket to the list of clients
            client_fds[client_cnt] = client_sock_fd;

            // Increase the current number of clients
            ++client_cnt;

            // Update the greatest descriptor currently used
            max_fd = MAX(max_fd, client_sock_fd);

            // Log some information about the connection
            char addr_string[INET6_ADDRSTRLEN];
            warnx(
                "A: %s\tT: %d\tS: %d\n",
                inet_ntop(
                    AF_INET6,
                    (void *) &(client_address.sin6_addr),
                    addr_string,
                    INET6_ADDRSTRLEN
                ),
                (int) time(NULL),
                client_sock_fd
            );
        }
    }

    return 0;
}
