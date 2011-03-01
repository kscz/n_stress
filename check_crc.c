#include <stdio.h>
#include "ccrc32.h"

int main(int argc, char *argv[])
{
    char stuff[512];
    int i;

    stuff[0]= 0xa5;
    for(i= 1;i < 512; i++) {
        stuff[i]= stuff[i-1] ^ i;
    }

    initializeCRC();

    printf("CRC32: %lx\n", fullCRC(stuff, 512));
}
