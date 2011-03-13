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
#include <netdb.h>
#include <pthread.h>
#include "packets.h"
#include "ccrc32.h"

/* Prototype(s) */
void make_socket_and_bind(unsigned int *, struct sockaddr_in *, int);
void make_socket_and_connect(unsigned int *, const char *, const char *);

/* {{{ main - sets up everything and starts the threads which spew
 * in: command line arguments
 * out: 0 on success
 */
int main(int argc, char *argv[]) {
    unsigned int sockfd, bindsockfd, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    unsigned int ret, pcount= 0;
    pthread_t send_thread, recv_thread;
    void *thr_ret;
    int ro= 0, wo= 0, client= 0, server= 0, portnum= 0;
    int i;

    /* Check if we have a valid number of arguments */
    if (argc < 2 || argc > 9) {
        fprintf(stderr,"Usage: %s [-l portnum] [-ro] [-wo] [-c host portnum] \
[-bs buffer size]\n", argv[0]);
        fprintf(stderr, "Network stress tester - sends packets with a \
repeating value and a crc32 until\nit receives a mismatch between the received\
 and calculated crc\n");
        fprintf(stderr,"\t-ro\n\t\tread only\n");
        fprintf(stderr,"\t-wo\n\t\twrite only\n");
        fprintf(stderr,"\t-l\n\t\tlisten on port# given\n");
        fprintf(stderr,"\t-c\n\t\tconnect to host on port number given\n");
        fprintf(stderr,"\t-bs\n\t\tchange the size of the packets\n");
        exit(1);
    }

    /* Go through and parse out all the arguments */
    for(i= 1;i < argc;i++) {
        if(strcmp(argv[i], "-l") == 0) {
            if(client) {
                fprintf(stderr, "Can't be both a server and a client!\n");
                exit(-1);
            }

            i++; /* Advance the counter to the portnum argument */
            if(i == argc) {
                fprintf(stderr, "No port argument given\n");
                exit(-1);
            }

            /* setup the info in the right spots */
            server= 1;
            portnum= atoi(argv[i]);
        } else if(strcmp(argv[i], "-c") == 0) {
            if(server) {
                fprintf(stderr, "Can't be both a server and a client!\n");
                exit(-1);
            }

            i++; /* Advance the counter to the hostname argument */
            if(i == argc) {
                fprintf(stderr, "No host or port argument given\n");
                exit(-1);
            }

            client= i; /* Save the location in the argv string of hostname */

            i++; /* Advance the counter to the portnum argument */
            if(i == argc) {
                fprintf(stderr, "No port argument given\n");
                exit(-1);
            }
            portnum= i; /* Save the location in the argv string of portnum */
        } else if(strcmp(argv[i], "-wo") == 0) {
            if(ro) {
                fprintf(stderr, "Must either read or write.\n");
                exit(-1);
            }

            wo= 1;
        } else if(strcmp(argv[i], "-ro") == 0) {
            if(wo) {
                fprintf(stderr, "Must either read or write.\n");
                exit(-1);
            }

            ro= 1;
        } else if(strcmp(argv[i], "-bs") == 0) {
            i++; /* Advance the counter to the buffer size argument */
            if(i == argc) {
                fprintf(stderr, "No buffer size given\n");
                exit(-1);
            }

            set_bufsize(atol(argv[i]));
        } else {
            fprintf(stderr, "%s: unknown argument\n", argv[i]);
            exit(-1);
        }
    }

    /* Initialize the CRC lookup table */
    initializeCRC();

    /* Setup sockfd to hold the needed info */
    if(server) {
        make_socket_and_bind(&bindsockfd, &serv_addr, portnum);
        listen(bindsockfd, 5); /* Start listening with 5 slots for waiting */

        fprintf(stderr, "Waiting for someone to connect...\n");
        clilen= sizeof(cli_addr); /* Wait for someone to connect */
        sockfd= accept(bindsockfd, (struct sockaddr *)&cli_addr, &clilen);
        if(sockfd < 0)
            error("ERROR on accept");

        fprintf(stderr, "Got connection!\n");
    } else if(client) {
        make_socket_and_connect(&sockfd, argv[client], argv[portnum]);
        fprintf(stderr, "Connected!\n");
    } else {
        fprintf(stderr, "You must be either client or server!\n");
        exit(-1);
    }

    /* This honestly won't do anything, it's just polite */
    /* I hoped it would make two write-only clients stop.  I was wrong */
    if(ro)
        shutdown(sockfd, SHUT_WR);
    if(wo)
        shutdown(sockfd, SHUT_RD);

    /* Try and make the threads */
    if(!ro) {
        ret= pthread_create(&send_thread, NULL, send_check, (void *)&sockfd);
        if(ret) {
            printf("Error creating send thread!\n");
            return ret;
        }
    }

    if(!wo) {
        ret= pthread_create(&recv_thread, NULL, recv_check, (void *)&sockfd);
        if(ret) {
            printf("Error creating receive thread!\n");
            return ret;
        }
    }

    /* Wait for the threads to complete */
    if(!ro) {
        ret= pthread_join(send_thread, (void **)&thr_ret);
        if(ret) {
            printf("Could not join the sending thread.\n");
            return ret;
        }

        pcount= *((unsigned int *)thr_ret);
        printf("Got bad packet after sending %u packets.\n", pcount);
    }

    if(!wo) {
        ret= pthread_join(recv_thread, (void **)&thr_ret);
        if(ret) {
            printf("Could not join the receiving thread.\n");
            return ret;
        }

        pcount= *((unsigned int *)thr_ret);
        printf("Got bad packet after receiving %u packets.\n", pcount);
    }

    /* Clean up the sockets when we're finished. This is us trying to be as
       nice and adherent to standards as possible. */
    if(ro) {
        shutdown(sockfd, SHUT_RD);
    } else if(wo) {
        shutdown(sockfd, SHUT_WR);
    } else {
        shutdown(sockfd, SHUT_RDWR);
    }

    /* Close the socket */
    close(sockfd);
    if(server)
        close(bindsockfd);

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
