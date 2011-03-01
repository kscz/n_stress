/** server.c - Server program designed to send and recieve a stream
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
#include <netinet/in.h>
#include <pthread.h>
#include "packets.h"
#include "ccrc32.h"

/* Prototype(s) */
void make_socket_and_bind(unsigned int *, struct sockaddr_in *, int);

/* {{{ main - sets up everything and starts the threads which spew
 * in: command line arguments
 * out: 0 on success
 */
int main(int argc, char *argv[]) {
    unsigned int sockfd, newsockfd, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    unsigned int ret, pcount= 0;
    pthread_t send_thread, recv_thread;
    void *thr_ret;

    if (argc < 2 || argc > 4) {
        fprintf(stderr,"Usage: %s portnum [buffer size]\n", argv[0]);
        exit(1);
    }

    /* Set up the buffer size if given */
    if(argc > 3)
        set_bufsize(atol(argv[2]));

    initializeCRC();

    make_socket_and_bind(&sockfd, &serv_addr, atoi(argv[1]));
    listen(sockfd, 5); /* Start listening with 5 slots for waiting */

    printf("Waiting for someone to connect...\n");
    clilen= sizeof(cli_addr); /* Wait for someone to connect */
    newsockfd= accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if(newsockfd < 0)
        error("ERROR on accept");

    printf("Got connection!\n");

    /* Try and make the threads */
    ret= pthread_create(&send_thread, NULL, send_check, (void *)&newsockfd);
    if(ret) {
        printf("Error creating send thread!\n");
        return ret;
    }

    ret= pthread_create(&recv_thread, NULL, recv_check, (void *)&newsockfd);
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

    pcount= *((unsigned int *)thr_ret);

    printf("Got bad packet after %u packets.\n", pcount);

    /* Clean up the sockets when we're finished */
    shutdown(newsockfd, SHUT_RDWR);
    close(sockfd);
    close(newsockfd);

    return 0;
} /* }}} */

/* {{{ make_socket_and_bind - creates and binds to a socket on portnum
 * in: pointer to an fd, the sockaddr struct, and the port number
 * out: nothing (exits on failure)
 */
void make_socket_and_bind(unsigned int *sockfd,
        struct sockaddr_in *serv_addr, int portnum)
{
    /* Generate the socket */
    *sockfd= socket(AF_INET, SOCK_STREAM, 0);
    if(*sockfd < 0) 
        error("ERROR opening socket!");

    /* Clean out the struct */
    memset((void *)serv_addr, 0, sizeof(*serv_addr));

    /* Set it up with the proper info */
    serv_addr->sin_family= AF_INET;
    serv_addr->sin_addr.s_addr= INADDR_ANY;
    serv_addr->sin_port= htons(portnum);

    /* Try and bind */
    if(bind(*sockfd, (struct sockaddr *)serv_addr,
        sizeof(*serv_addr)) < 0) 
        error("ERROR on binding");
} /* }}} */
