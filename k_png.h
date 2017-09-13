//----------------------------------------------------------------------------------------------------------------------
// PNG writing (uncompressed)
// For now, use stb_image for loading images
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_blob.h>
#include <kore/k_memory.h>
#include <kore/k_crc32.h>

bool pngWrite(const char* fileName, u32* img, int width, int height);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

#define K_DEFLATE_MAX_BLOCK_SIZE    65536
#define K_BLOCK_HEADER_SIZE         5

internal u32 __pngAdler32(u32 state, const u8* data, i64 len)
{
    u16 s1 = state;
    u16 s2 = state >> 16;
    for (i64 i = 0; i < len; ++i)
    {
        s1 = (s1 + data[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (u32)s2 << 16 | s1;
}

bool pngWrite(const char* fileName, u32* img, int width, int height)
{
    // Swizzle image from ARGB to ABGR
    u32* newImg = K_ALLOC(sizeof(u32)*width*height);
    u8* src = (u8 *)img;
    u8* dst = (u8 *)newImg;

    // Data is BGRABGRA...  should be RGBARGBA...
    for (int yy = 0; yy < height; ++yy)
    {
        for (int xx = 0; xx < width; ++xx)
        {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = src[3];
            src += 4;
            dst += 4;
        }
    }
    img = newImg;

    // Calculate size of PNG
    i64 fileSize = 0;
    i64 lineSize = width * sizeof(u32) + 1;
    i64 imgSize = lineSize * height;
    i64 overheadSize = imgSize / K_DEFLATE_MAX_BLOCK_SIZE;
    if (overheadSize * K_DEFLATE_MAX_BLOCK_SIZE < imgSize)
    {
        ++overheadSize;
    }
    overheadSize = overheadSize * 5 + 6;
    i64 dataSize = imgSize + overheadSize;      // size of zlib + deflate output
    u32 adler = 1;
    i64 deflateRemain = imgSize;
    u8* imgBytes = (u8 *)img;

    fileSize = 43;
    fileSize += dataSize + 4;       // IDAT deflated data

    // Open arena
    Arena m;
    arenaInit(&m, dataSize + KB(1));
    u8* p = arenaAlloc(&m, 43);
    u8* start = p;

    // Write file format
    u8 header[] = {
        // PNG header
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        // IHDR chunk
        0x00, 0x00, 0x00, 0x0d,                                 // length
        0x49, 0x48, 0x44, 0x52,                                 // 'IHDR'
        width >> 24, width >> 16, width >> 8, width,            // width
        height >> 24, height >> 16, height >> 8, height,        // height
        0x08, 0x06, 0x00, 0x00, 0x00,                           // 8-bit depth, true-colour+alpha format
        0x00, 0x00, 0x00, 0x00,                                 // CRC-32 checksum
        // IDAT chunk
        (u8)(dataSize >> 24), (u8)(dataSize >> 16), (u8)(dataSize >> 8), (u8)dataSize,
        0x49, 0x44, 0x41, 0x54,                                 // 'IDAT'
        // Deflate data
        0x08, 0x1d,                                             // ZLib CMF, Flags (Compression level 0)
    };
    memoryCopy(header, p, sizeof(header) / sizeof(header[0]));
    u32 crc = crc32(&p[12], 17);
    p[29] = crc >> 24;
    p[30] = crc >> 16;
    p[31] = crc >> 8;
    p[32] = crc;
    crc = crc32(&p[37], 6);

    // Write out the pixel data compressed
    int x = 0;
    int y = 0;
    i64 count = width * height * sizeof(u32);
    i64 deflateFilled = 0;
    while (count > 0)
    {
        // Start DEFALTE block
        if (deflateFilled == 0)
        {
            u32 size = K_DEFLATE_MAX_BLOCK_SIZE;
            if (deflateRemain < (i64)size)
            {
                size = (u16)deflateRemain;
            }
            u8 blockHeader[K_BLOCK_HEADER_SIZE] = {
                deflateRemain <= K_DEFLATE_MAX_BLOCK_SIZE ? 1 : 0,
                size,
                size >> 8,
                (size) ^ 0xff,
                (size >> 8) ^ 0xff
            };
            p = arenaAlloc(&m, sizeof(blockHeader));
            memoryCopy(blockHeader, p, sizeof(blockHeader));
            crc = crc32Update(crc, blockHeader, sizeof(blockHeader));
        }

        // Calculate number of bytes to write in this loop iteration
        u32 n = 0xffffffff;
        if ((u32)count < n)
        {
            n = (u32)count;
        }
        if ((u32)(lineSize - x) < n)
        {
            n = (u32)(lineSize - x);
        }
        K_ASSERT(deflateFilled < K_DEFLATE_MAX_BLOCK_SIZE);
        if ((u32)(K_DEFLATE_MAX_BLOCK_SIZE - deflateFilled) < n)
        {
            n = (u32)(K_DEFLATE_MAX_BLOCK_SIZE - deflateFilled);
        }
        K_ASSERT(n != 0);

        // Beginning of row - write filter method
        if (x == 0)
        {
            p = arenaAlloc(&m, 1);
            *p = 0;
            crc = crc32Update(crc, p, 1);
            adler = __pngAdler32(adler, p, 1);
            --deflateRemain;
            ++deflateFilled;
            ++x;
            if (count != n) --n;
        }

        // Write bytes and update checksums
        p = arenaAlloc(&m, n);
        memoryCopy(imgBytes, p, n);
        crc = crc32Update(crc, imgBytes, n);
        adler = __pngAdler32(adler, imgBytes, n);
        imgBytes += n;
        count -= n;
        deflateRemain -= n;
        deflateFilled += n;
        if (deflateFilled == K_DEFLATE_MAX_BLOCK_SIZE)
        {
            deflateFilled = 0;
        }
        x += n;
        if (x == lineSize) {
            x = 0;
            ++y;
            if (y == height)
            {
                // Wrap things up
                u8 footer[20] = {
                    adler >> 24, adler >> 16, adler >> 8, adler,    // Adler checksum
                    0, 0, 0, 0,                                     // Chunk crc-32 checksum
                    // IEND chunk
                    0x00, 0x00, 0x00, 0x00,
                    0x49, 0x45, 0x4e, 0x44,
                    0xae, 0x42, 0x60, 0x82,
                };
                crc = crc32Update(crc, footer, 20);
                footer[4] = crc >> 24;
                footer[5] = crc >> 16;
                footer[6] = crc >> 8;
                footer[7] = crc;

                p = arenaAlloc(&m, 20);
                memoryCopy(footer, p, 20);
                break;
            }
        }
    }

    // Transfer file
    K_FREE(newImg, sizeof(u32)*width*height);
    u8* end = arenaAlloc(&m, 0);
    i64 numBytes = (i64)(end - start);
    Blob b = blobMake(fileName, numBytes);
    if (b.bytes)
    {
        memoryCopy(start, b.bytes, numBytes);
        blobUnload(b);
        arenaDone(&m);
        return YES;
    }
    else
    {
        return NO;
    }
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif
