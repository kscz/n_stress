/* Function prototypes */
#ifndef __PACKETS_H
    #define __PACKETS_H
    #include <stdint.h>

    int parse_packet(const unsigned char *buffer, size_t length,
            uint32_t g_crc);
    int generate_packet(unsigned char *buffer, size_t buffersize);
    void print_packet(const unsigned char *buffer, size_t length);
    int set_bufsize(size_t new_sz);

    void *send_check(void *fd);
    void *recv_check(void *fd);

    void error(const char *msg);
#endif
