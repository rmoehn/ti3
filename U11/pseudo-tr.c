#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include "errors.h"
#include <string.h>
#include <signal.h>

#define BUFSIZE 1024
#define D_PORT 80
#define MAX_TTL 30
#define DATA "What-ho!"

volatile sig_atomic_t got_SIGCHLD = 0;
void handle_SIGCHLD(int sig_num)
{
    got_SIGCHLD = 1;
}

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 2) {
        errx(ERR_ARG, "Usage: pseudo-tr <destination ip address>");
    }
    char *final_address = argv[1];

    // Receive ICMP packets in the child
    pid_t pid;
    if ((pid = fork()) == 0) {
        // Open socket for receiving
        int in_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (in_sock == -1) {
            err(ERR_SOCKET, "Cannot open raw socket");
        }

        // Receive all ICMP packets sent to this computer
        struct sockaddr_in sock_addr;
        socklen_t sock_addr_len;
        char buffer[BUFSIZE];
        while (1) {
            // Receive data
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

            // Extract the IP header and the ICMP packet's beginning
            struct iphdr *ip_header = (struct iphdr *) buffer;
            int icmp_offs           = ip_header->ihl * 4;

            // Extract the ICMP header and the UDP packet's beginning
            struct icmphdr *icmp_header
                = (struct icmphdr *) (buffer + icmp_offs);
            int udp_offs = icmp_offs + 4;

            // Extract the UDP header
            struct udphdr *udp_header = (struct udphdr *) (buffer + udp_offs);

            // If it is the port where we sent the UDP packet out
            //if (udp_header->dest == D_PORT) {
                // Print the ICMP packet's sender's IP address
                struct in_addr ipa;
                ipa.s_addr       = ip_header->saddr;
                char *ip_address = inet_ntoa(ipa);
                printf("%s\n", ip_address);

                // Stop if we got through to the destination
                if (strcmp(ip_address, final_address) == 0) {
                    if (close(in_sock) == -1) {
                        err(ERR_SOCKET, "Problem closing socket");
                    }
                    exit(0);
                }
            //}
        }
    }
    // Send UDP packets in the parent
    else if (pid != -1) {
        // Install a handler for SIGCHLD
        struct sigaction SIGCHLD_action;
        SIGCHLD_action.sa_handler = handle_SIGCHLD;
        SIGCHLD_action.sa_flags   = 0;
        sigfillset( &SIGCHLD_action.sa_mask );
        if (sigaction(SIGCHLD, &SIGCHLD_action, NULL) == -1) {
            err(ERR_SIGNAL, "Cannot install handler for SIGCHLD");
        }

        // Open socket for sending
        int out_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (out_sock == -1) {
            err(ERR_SOCKET, "Cannot open output socket");
        }

        // Construct the address for sending
        struct sockaddr_in sock_addr;
        sock_addr.sin_family = AF_INET;
        sock_addr.sin_port   = htons(D_PORT);
        if (inet_aton(final_address, &(sock_addr.sin_addr)) == 0) {
            errx(ERR_ARG, "Invalid IP address: %s", final_address);
        }

        // With increasing TTLs
        for (int ttl = 1; ttl <= MAX_TTL; ++ttl) {
            // Exit if child says that we already have reached the destination
            if (got_SIGCHLD) {
                if (close(out_sock) == -1) {
                    err(ERR_SOCKET, "Problem closing socket");
                }

                exit(0);
            }

            // Set this TTL on the socket
            if (setsockopt(out_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(int))
                    == -1) {
                err(ERR_SOCKET, "Cannot set TTL");
            }

            // Send some data to it
            char *data     = DATA;
            size_t datalen = strlen(data) + 1;
            int sendto_ret = sendto(
                                 out_sock,
                                 data,
                                 datalen,
                                 0,
                                 (struct sockaddr *) &sock_addr,
                                 sizeof(struct sockaddr_in)
                             );
            if (sendto_ret != datalen) {
                err(ERR_SEND, "Problem sending to socket");
            }

            // Wait a sec
            sleep(1);
        }
    }
    // Abort on error
    else {
        err(ERR_FORK, "Cannot fork");
    }

    return 0;
}
