#ifndef _CCRC32_H
#define _CCRC32_H
    #include <stdint.h>

    void initializeCRC(void);
    uint32_t fullCRC(const unsigned char *sData, uint32_t ulDataLength);
    void partialCRC(uint32_t *ulCRC, const unsigned char *sData, uint32_t ulDataLength);
#endif
