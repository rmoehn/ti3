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

    // Create a socket for reception
    int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        err(SOCK_ERROR, "Cannot open socket");
    }

    // Create structure for binding the socket to the specified filename
    struct sockaddr_un mine_addr;
    memset(&mine_addr, 0, sizeof(struct sockaddr_un));
    mine_addr.sun_family = AF_UNIX;
    strncpy(mine_addr.sun_path, argv[1], sizeof(mine_addr.sun_path) - 1);
        // Safest, but might be done differently

    // Bind the socket to the filename specified at the command line
    int bind_ret = bind(
                       sock_fd,
                       (struct sockaddr *) &mine_addr,
                       sizeof(struct sockaddr_un)
                   );
    if (bind_ret == -1) {
        err(SOCK_ERROR, "Cannot bind socket to %s", argv[1]);
    }

    // Read what is sent until the end of the world
    char buffer[BUFSIZE];
    int recv_ret;
    while ((recv_ret = recv(sock_fd, buffer, BUFSIZE, 0)) > 0) {
        // Write it to standard output
        fputs(buffer, stdout);
    }
    if (recv_ret <= 0) {
        err(RECV_ERROR, "Error in reception");
    }

    return 0;
}
