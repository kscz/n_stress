#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ccrc32.h"

uint32_t reflect(uint32_t ulReflect, const char cChar);
static uint32_t ulTable[256]; // CRC lookup table array.

/* This function initializes "CRC Lookup Table". You only need to call it once to
   initalize the table before using any of the other CRC32 calculation functions.
*/

void initializeCRC(void)
{
    int iCodes, iPos;
    //0x04C11DB7 is the official polynomial used by PKZip, WinZip and Ethernet.
    uint32_t ulPolynomial = 0x04C11DB7;

    //memset(&this->ulTable, 0, sizeof(this->ulTable));

    // 256 values representing ASCII character codes.
    for(iCodes= 0; iCodes <= 0xFF; iCodes++)
    {
        ulTable[iCodes]= reflect(iCodes, 8) << 24;

        for(iPos= 0; iPos < 8; iPos++)
        {
            ulTable[iCodes]= (ulTable[iCodes] << 1)
                ^ ((ulTable[iCodes] & (1 << 31)) ? ulPolynomial : 0);
        }

        ulTable[iCodes]= reflect(ulTable[iCodes], 32);
    }
}

/*
    Reflection is a requirement for the official CRC-32 standard.
    You can create CRCs without it, but they won't conform to the standard.
*/

uint32_t reflect(uint32_t ulReflect, const char cChar)
{
    uint32_t ulValue = 0;
    int iPos;

    // Swap bit 0 for bit 7, bit 1 For bit 6, etc....
    for(iPos = 1; iPos < (cChar + 1); iPos++)
    {
        if(ulReflect & 1)
        {
            ulValue |= (1 << (cChar - iPos));
        }
        ulReflect >>= 1;
    }

    return ulValue;
}

/*
    Calculates the CRC32 by looping through each of the bytes in sData.
*/

void partialCRC(uint32_t *ulCRC, const unsigned char *sData,
        uint32_t ulDataLength)
{
    while(ulDataLength--)
    {
        //If your compiler complains about the following line, try changing each
        //    occurrence of *ulCRC with "((uint32_t)*ulCRC)" or "*(uint32_t *)ulCRC".

         *(uint32_t *)ulCRC =
            ((*(uint32_t *)ulCRC) >> 8) ^ ulTable[((*(uint32_t *)ulCRC) & 0xFF) ^ *sData++];
    }
}

/*
    Returns the calculated CRC23 for the given string.
*/

uint32_t fullCRC(const unsigned char *sData, uint32_t ulDataLength)
{
    uint32_t ulCRC = 0xffffffff; //Initilaize the CRC.
    partialCRC(&ulCRC, sData, ulDataLength);
    return(ulCRC ^ 0xffffffff); //Finalize the CRC and return.
}
