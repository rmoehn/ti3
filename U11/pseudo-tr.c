#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <err.h>
#include <stdlib.h>
#include "errors.h"

#define BUFSIZE 1024

int main(int argc, char *argv[])
{
    int in_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (in_sock == -1) {
        err(ERR_SOCKET, "Cannot open raw socket");
    }

    struct sockaddr_in sock_addr;
    socklen_t sock_addr_len;
    //sock_addr.sin_family = AF_INET;
    //sock_addr.sin_port   = IPPROTO_ICMP;
    //sock_addr.sin_addr.s_addr   = INADDR_ANY;

    char buffer[BUFSIZE];

    while (1) {
        int rcvf_ret = recvfrom(
                           in_sock,
                           buffer,
                           BUFSIZE,
                           0,
                           (struct sockaddr *) &sock_addr,
                           &sock_addr_len
                       );

        if (rcvf_ret == 0) {
            exit(0);
        }

        if (rcvf_ret == -1) {
            err(ERR_RECV, "Error receiving from ICMP socket");
        }

        printf("Typ: %d Code: %d Checksum: %d\n",
            (int) buffer[0],
            (int) buffer[1],
            (int) ((buffer[2] << 8) | buffer[3])
        );

        puts(buffer);
        fflush(stdout);
    }

    return 0;
}
