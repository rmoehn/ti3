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
#include <glib.h>
#include <sys/stat.h>
#include "errors.h"

#define STATUS_200 "200 OK"
#define STATUS_414 "414 Request-URI Too Large"
#define STATUS_501 "501 Not Implemented"
#define STATUS_505 "505 HTTP Version not supported"
#define STATUS_404 "404 Not Found"
#define STATUS_500 "500 Internal Server Error"
#define STATUS_415 "415 Unsupported Media Type"

#define BACKLOG_SIZE 7
#define MAX_CLIENT_CNT 50
#define MAX_REQUEST_LINE_LEN 500
#define MAX_MIME_LEN 10
#define BUFSIZE 4096

void respond(char *, int);
void register_in_set(gpointer, gpointer);
void close_fd(gpointer, gpointer);

volatile sig_atomic_t got_SIGINT = 0;
void handle_SIGINT(int sig_num)
{
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
    GSList *client_fds = NULL;
    int client_cnt = 0;

    // React to events on the watched sockets
    while (1) {
        // Watch the socket to accept calls on
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_fd, &readfds);

        // Put the current clients under surveillance
        g_slist_foreach(client_fds, register_in_set, &readfds);

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
            g_slist_foreach(client_fds, close_fd, &is_proper_shutdown);

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
            client_fds = g_slist_prepend(
                             client_fds,
                             GINT_TO_POINTER(client_sock_fd)
                         );

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

        // Go through all registered clients
        for (GSList *cur_client = client_fds;
                cur_client != NULL;
                cur_client = g_slist_next(cur_client)) {
            int client_fd = GPOINTER_TO_INT(cur_client->data);

            // If there is a request
            char request_line[MAX_REQUEST_LINE_LEN + 1];
            if (FD_ISSET(client_fd, &readfds)) {
                // Read some data from it
                int read_bytes_cnt
                    = read(client_fd, request_line, MAX_REQUEST_LINE_LEN);

                // Proceed if the client had nothing to say after all
                if (read_bytes_cnt == 0) {
                    warnx(
                        "Immediate end of file at descriptor %d",
                        client_fd
                    );
                    continue;
                }

                // Close the socket if there was an error
                if (read_bytes_cnt == -1) {
                    warn("Error at descriptor %d", client_fd);
                    warnx("Trying to close it.");

                    // Issue a warning if it cannot be closed
                    if (close(client_fd) == -1) {
                        warn("Error closing descriptor %d", client_fd);
                    }
                }

                // Remove the client from the wait list
                client_fds = g_slist_remove(client_fds, cur_client->data);
                --client_cnt;

                // Turn the received data into a proper string
                *(request_line + MAX_REQUEST_LINE_LEN) = '\0';

                // Check whether we received the whole request line
                char *end_of_rline = strchr(request_line, '\n');
                if (end_of_rline == NULL) {
                    respond(STATUS_414, client_fd);
                    continue;
                }

                // Chop off anything beyond the request line
                *(end_of_rline + 1) = '\0';
                warnx("Received request: %s", request_line);

                // Reject everything that isn't a GET request
                char *uri  = strchr(request_line, ' ') + 1;
                *(uri - 1) = '\0';
                if (strcmp(request_line, "GET") != 0) {
                    respond(STATUS_501, client_fd);
                    continue;
                }

                // Reject non-HTTP/1.1 requests
                char *http_version  = strchr(uri, ' ');
                *(http_version - 1) = '\0';
                if (strcmp(http_version, "HTTP/1.1") != 0) {
                    respond(STATUS_505, client_fd);
                    continue;
                }

                // Get information about the requested file
                struct stat statbuf;
                if (stat(uri, &statbuf) == -1) {
                    if (errno == ENOENT) {
                        warnx(
                            "Descriptor %d requested nonexistent file",
                            client_fd
                        );
                        respond(STATUS_404, client_fd);
                    }
                    else {
                        warn("stat error with file %s", uri);
                        respond(STATUS_500, client_fd);
                    }

                    continue;
                }

                // Guess the MIME type of the file from the file name
                char mime_type[MAX_MIME_LEN + 1];
                char *filename_ext = http_version - 4;
                if (strcmp(filename_ext, "tml") == 0
                        || strcmp(filename_ext, "htm") == 0) {
                    strcpy(mime_type, "text/html");
                }
                else if (strcmp(filename_ext, "peg") == 0
                        || strcmp(filename_ext, "jpg") == 0) {
                    strcpy(mime_type, "image/jpeg");
                }
                else if (strcmp(filename_ext, "gif") == 0) {
                    strcpy(mime_type, "image/gif");
                }
                else {
                    warnx(
                        "Descriptor %d requested file with unknown extension"
                        " %s.",
                        client_fd,
                        filename_ext
                    );
                    respond(STATUS_415, client_fd);
                    continue;
                }

                // Build the response header
                int header_len = 8 + 1 + 3 + 1 + 2 + 1
                                 + 13 + 1 + MAX_MIME_LEN + 1
                                 + 17 + 1
                                 + 15 + 1 + 20 + 1
                                 + 1;
                char header[header_len + 1];
                memset(header, 0, header_len + 1);
                sprintf(
                    header,
                    "HTTP/1.1 200 OK\n"
                    "Content-Type: %s\n"
                    "Connection: close\n"
                    "Content-Length: %ld\n"
                    "\n",
                    mime_type,
                    (long) statbuf.st_size
                );

                // Send it
                if (send(client_fd, header, header_len + 1, 0)
                        != header_len + 1) {
                    warn(
                        "Could not send the whole header to descriptor %d",
                        client_fd
                    );
                    continue;
                }

                // Open the file to be sent
                FILE *infile = fopen(uri, "r");
                if (infile == NULL) {
                    if (errno == ENOENT) {
                        warnx("File %s has disappeared.", uri);
                        respond(STATUS_404, client_fd);
                    }
                    else {
                        warn("Cannot open %s for reading", uri);
                        respond(STATUS_500, client_fd);
                    }

                    continue;
                }

                // Send the contents to the client
                char buf[BUFSIZE];
                size_t bytes_read;
                while ((bytes_read = fread(buf, BUFSIZE, 1, infile)) != 0) {
                    if (send(client_fd, buf, bytes_read, 0) != bytes_read) {
                        warn("Problem sending to descriptor %d", client_fd);
                    }
                }
                if (ferror(infile)) {
                    warn("Cannot read from %s", uri);
                    respond(STATUS_500, client_fd);
                }

                // Close the input file
                if (fclose(infile) == EOF) {
                    warn("Problem closing %s", uri);
                }

                // Close the connection to the client
                warnx("Closing descriptor %d.", client_fd);
                if (close(client_fd) == -1) {
                    warn("Cannot close descriptor %d", client_fd);
                }
            }
        }
    }

    return 0;
}

/*
 * Set the file descriptor client_fd in the file descriptor set pointed to by
 * readfds_pt.
 */
void register_in_set(gpointer client_fd, gpointer readfds_pt)
{
    FD_SET(GPOINTER_TO_INT(client_fd), (fd_set *) readfds_pt);
}

/*
 * Closes the file behind descriptor client_fd and sets is_proper_shutdown to
 * 0 if there were problems.
 */
void close_fd(gpointer client_fd, gpointer is_proper_shutdown)
{
    if (close(GPOINTER_TO_INT(client_fd)) == -1) {
        warn("Problem closing client socket");
        *((int *) is_proper_shutdown) = 0;
    }
}

/*
 * Sends the HTTP status line in status_line to the socket descriptor sock_fd
 * and closes it.
 */
void respond(char *status_msg, int sock_fd)
{
    // Build the status line
    int status_line_len = 8 + 1 + strlen(status_msg);
    char status_line[status_line_len + 1];
    strcpy(status_line, "HTTP/1.1 ");
    strcat(status_line, status_msg);
    strcat(status_line, "\n");

    // Send it
    if (send(sock_fd, status_line, status_line_len + 1, 0) != status_line_len)
            {
        warn(
            "Could not send the whole status line to descriptor %d",
            sock_fd
        );
    }

    // Close the socket
    if (close(sock_fd) == -1) {
        warn("Error closing descriptor %d", sock_fd);
    }
}
