#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include "errors.h"

#define BUFSIZE 1024

int main(int argc, const char *argv[])
{
    // Check number of arguments
    if (argc != 2) {
        errx(ARG_ERROR, "Socket file path must be only argument");
    }

    // Create a socket for sending
    int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        err(SOCK_ERROR, "Cannot open socket");
    }

    // Create structure identifying the receiver
    struct sockaddr_un thine_addr;
    memset (&thine_addr, 0, sizeof(struct sockaddr_un));
    thine_addr.sun_family = AF_UNIX;
    strncpy(thine_addr.sun_path, argv[1], sizeof(thine_addr.sun_path) - 1);
        // Safest, but might be done differently

    // Read from standard input
    char buffer[BUFSIZE];
    while (fgets(buffer, BUFSIZE, stdin) != NULL) {
        // Send the data to the server
        int sendto_ret = sendto(
                             sock_fd,
                             buffer,
                             BUFSIZE,
                             0,
                             (struct sockaddr *) &thine_addr,
                             sizeof(struct sockaddr_un)
                         );
        if (sendto_ret == -1) {
            err(SEND_ERROR, "Cannot send to %s", argv[1]);
        }
    }
    if (ferror(stdin)) {
        err(INPUT_ERROR, "Error reading from standard input");
    }

    return 0;
}
