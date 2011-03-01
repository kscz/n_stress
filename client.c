/** client.c - Client program designed to send and recieve a stream
 * of arbitrary bytes, compute the crc32, check that it matches
 * the received crc32, then replies with its own stream of bytes
 * with a crc32
 *
 * Every packet should be prefixed with 0xAA55AA55AA55AA55
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include "packets.h"
#include "ccrc32.h"

/* Prototype(s) */
void make_socket_and_connect(unsigned int *, const char *, const char *);

/* {{{ main - sets all the little pieces up then simply starts the blaster
 * in: command line arguments
 * out: 0 on success
 */
int main(int argc, char *argv[]) {
    unsigned int sockfd;
    unsigned int ret, pcount;
    pthread_t send_thread, recv_thread;
    void *thr_ret;

    if (argc < 3 || argc > 5) {
        fprintf(stderr,"Usage: %s hostname portnum [buffer size]\n", argv[0]);
        exit(1);
    }

    /* Set up the buffer size if given */
    if(argc > 4)
        set_bufsize(atol(argv[3]));

    initializeCRC();

    make_socket_and_connect(&sockfd, argv[1], argv[2]);
    printf("Connected!\n");

    /* Try and make the threads */
    ret= pthread_create(&send_thread, NULL, send_check, (void *)&sockfd);
    if(ret) {
        printf("Error creating send thread!\n");
        return ret;
    }

    ret= pthread_create(&recv_thread, NULL, recv_check, (void *)&sockfd);
    if(ret) {
        printf("Error creating receive thread!\n");
        return ret;
    }

    /* Wait for the threads to complete */
    ret= pthread_join(send_thread, (void **)&thr_ret);
    if(ret) {
        printf("Could not join the sending thread.\n");
        return ret;
    }

    ret= pthread_join(recv_thread, (void **)&thr_ret);
    if(ret) {
        printf("Could not join the receiving thread.\n");
        return ret;
    }

    /* Get the number of packets received */
    pcount= *((unsigned int *)thr_ret);

    printf("Got bad packet after %u packets.\n", pcount);

    /* Clean up the TCP sockets */
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
} /* }}} */

/* {{{ make_socket_and_connect - make a socket fd and connect it to the host
 * in: pointer to the fd to fill, the string with the server name, and the port #
 * out: nothing (exits on failure)
 */
void make_socket_and_connect(unsigned int *sockfd, const char* server,
        const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;

    /* Try and get the host */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family= AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype= SOCK_STREAM; /* TCP stream */
    hints.ai_flags= 0;
    hints.ai_protocol= 0; /* Any protocol */

    if((s= getaddrinfo(server, port, &hints, &result)) != 0) {
        fprintf(stderr, "couldn't get address info: %s\n", gai_strerror(s));
        exit(-1);
    }

    /* Now we begin traversing the linked list of address structs */
    for(rp= result;rp != NULL;rp= rp->ai_next) {
        *sockfd= socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(*sockfd < 0)
            continue;

        if(connect(*sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break; /* Connected! */
        }

        close(*sockfd); /* Couldn't connect so close it */
    }

    if(rp == NULL) {
        fprintf(stderr, "Couldn't connect and/or generate socket.\n");
        exit(-2);
    }

    /* We're all connected, go ahead and return. */
    freeaddrinfo(result);
} /* }}} */
