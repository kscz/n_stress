#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include "packets.h"
#include "ccrc32.h"

/* Defines the default buffer size */
#define DEFAULT_BUFSZ 0x10000

/* The global variables */
static int lastgood= 1; /* Whether the last packet received was valid */
static unsigned int preccount= 0, psndcount= 0; /* Packet sent/received count */
static size_t bufsize= DEFAULT_BUFSZ; /* The buffer size reference throughout */

/* Some stuff which is easy to identify in a packet dump */
static const char stuff[]= {0xAB, 0xAD, 0xBA, 0xBE,
                            0xCA, 0xFE, 0xF0, 0x0D,
                            0xDE, 0xAD, 0xBE, 0xEF,
                            0xDE, 0xFE, 0xC8, 0xED,
                            0x01, 0x23, 0x45, 0x67,
                            0x89, 0xAB, 0xCD, 0xEF};

/* error - use perror to display a message then exit
 * in:  a string to display
 * out: does not return
 */
void error(const char *msg) {
    perror(msg);
    exit(1);
}

/* set_bufsize - set the buffer size which will be used
 * in:  a size
 * out: 0 on success
 */
int set_bufsize(size_t new_sz) {
    /* Set the new bufsize */
    bufsize= new_sz;

    return 0;
}

/* parse_packet - takes in a packet and checks that the first 8 bytes match
 *                then checks that the CRCs match
 * in:  a pointer to a packet and the length of that packet, optionally a crc (or 0)
 * out: 0 on success
 */
int parse_packet(const unsigned char *buffer, size_t length, uint32_t g_crc)
{
    int i;
    uint32_t crc, reccrc;

    /* Check that we at least have the handshake */
    if(length < 8)
        return -1;

    /* Check that the "handshake" is valid */
    for(i= 0;i < 8;i+= 2) {
        if(buffer[i] != 0xAA || buffer[i+1] != 0x55) {
            printf("Bad handshake\n");
            return -2;
        }
    }

    /* Read in the crc which we received */
    reccrc= buffer[length - 4];
    reccrc= (reccrc << 8) | buffer[length - 3];
    reccrc= (reccrc << 8) | buffer[length - 2];
    reccrc= (reccrc << 8) | buffer[length - 1];

    if(g_crc == 0) {
        /* Compute the crc */
        crc= fullCRC(buffer, length - 4);
    } else
        crc= g_crc;

    if(reccrc != crc) {
        printf("crc32 differed.\nreceived: %"PRIx32"\nbut calculated:  %"PRIx32"\n",
                reccrc, crc);
        return -3;
    }

    return 0;
}

/* generate_packet - make a packet which conforms to our "rules"
 * in: a buffer to store the packet in, and its size
 * out: 0 on success
 */
int generate_packet(unsigned char *buffer, size_t buffersize)
{
    size_t i;
    uint32_t crc;
    static int currentjunk= 0; /* keep track of the current filler bytes */

    if(buffersize < 12) {
        printf("Can't generate packet, buffer too small!\n");
        return -1;
    }

    /* Fill in the "handshake" */
    for(i= 0;i < 8;i+= 2) {
        buffer[i]= 0xAA;
        buffer[i+1]= 0x55;
    }

    /* fill the buffer with a repeating set of bytes */
    for(i= 8;i < buffersize - 4;i+= 4) {
        buffer[i]= stuff[currentjunk];
        buffer[i + 1]= stuff[currentjunk + 1];
        buffer[i + 2]= stuff[currentjunk + 2];
        buffer[i + 3]= stuff[currentjunk + 3];
    }

    /* Make the next set of junk different (cycle through "stuff") */
    currentjunk= (currentjunk + 4) % (sizeof(stuff)/sizeof(*stuff));;

    /* Calculate the CRC */
    crc= fullCRC(buffer, buffersize - 4);

    /* Put the CRC at the end of the packet */
    i= (buffersize - 4);
    buffer[i++]= (crc >> 24) & 0xff;
    buffer[i++]= (crc >> 16) & 0xff;
    buffer[i++]= (crc >> 8) & 0xff;
    buffer[i++]= (crc & 0xff);

    return 0;
}

/* print_packet - dump out all the bytes of the packet for inspection
 * in:  buffer of chars to print, and the number of chars
 * out: nothing
 */
void print_packet(const unsigned char *buffer, size_t length)
{
    size_t i;

    printf("Packet dump:");

    /* Loop through and print every 48 chars to a line */
    for(i= 0;i < length;i++) {
        if(i % 48 == 0)
            printf("\n");

        printf("%02hhx", *(buffer++));
    }

    printf("\n\n");
}

/* send_check - The function which simply loops dumping packets into fd
 * in: pointer to a file descriptor to dump to
 * out: nothing
 */
void *send_check(void *fd)
{
    int ourfd= *(int *)fd;
    ssize_t ret, n, max= (bufsize >> 1);
    unsigned char *buffer= malloc(bufsize * sizeof(*buffer));

    if(buffer == NULL) {
        printf("Could not allocate space for send buffer!\n");
        return NULL;
    }

    /* Loop until we get bad data */
    while(lastgood) {
        n= 0;

        generate_packet(buffer, (bufsize >> 1));

        /* We may not write all the data in one go so loop */
        do {
            n+= ret= write(ourfd, buffer + n, max - n);
        } while(n < max && ret > 0);

        psndcount++;

        if(ret == 0) {
            fprintf(stderr, "Stopped writing!\n");
            break;
        }

        if(ret < 0)
            error("ERROR writing to socket\n");
    }

    free(buffer); /* Clean up after ourselves! */

    return (void *)&psndcount;
}

/* recv_check - Function called by pthread_create to receive and check packets
 * in:  a pointer to a file descriptor
 * out: the number of packets received, or 0xFFFF on failure
 */
void *recv_check(void *fd)
{
    int ourfd= *(int *)fd;
    ssize_t n = 0, ret = 1, max= (bufsize >> 1);
    unsigned char *buffer= malloc(bufsize * sizeof(*buffer));

    if(buffer == NULL) {
        printf("Could not allocate space for receive buffer!\n");
        preccount= -1;
        return (void *)&preccount;
    }

    preccount= 0;

    /* Loop until we receive bad data */
    while(lastgood) {
        n= 0;

        /* We may not read a whole packet in one read, so loop */
        do {
            n+= ret= read(ourfd, buffer + n, max - n);
        } while(n < max && ret > 0);

        if(ret < 0)
            error("ERROR reading from socket\n");

        preccount++; /* indicate we got another packet */
        if(n < max) {
            printf("Got truncated packet.\n");
            lastgood= 0;

            break;
        }

        lastgood= !parse_packet(buffer, n, 0); /* check if it's good, doing full CRC */
    }

    /* If we made it here then the packet was bad */
    /* Print the packet for analysis */
    print_packet(buffer, n);

    free(buffer); /* Clean up after ourselves! */

    /* return the number of packets we got */
    return (void *)&preccount;
}
