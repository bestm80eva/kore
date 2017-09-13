//----------------------------------------------------------------------------------------------------------------------
// CRC-32 algorithm
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>

// Checksum calculation
u32 crc32(void* data, i64 len);
u32 crc32Update(u32 crc, void* data, i64 len);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

// Table of CRCs of all 8-bit messages.
unsigned long kCrcTable[256];

// Flag: has the table been computed? Initially false.
bool gCrcTableComputed = NO;

// Make the table for a fast CRC.
void __crcMakeTable(void)
{
    u32 c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (u32)n;
        for (k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        kCrcTable[n] = c;
    }
    gCrcTableComputed = YES;
}

// Update a running CRC with the bytes data[0..len-1]--the CRC
// should be initialized to all 1's, and the transmitted value
// is the 1's complement of the final running CRC (see the
// crc() routine below). */

u32 crc32Update(u32 crc, void* data, i64 len)
{
    u32 c = crc;
    u8* d = (u8 *)data;

    if (!gCrcTableComputed) __crcMakeTable();
    for (i64 n = 0; n < len; n++) {
        c = kCrcTable[(c ^ d[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
u32 crc32(void* data, i64 len)
{
    return crc32Update(0xffffffffL, data, len) ^ 0xffffffffL;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
