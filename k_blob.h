//----------------------------------------------------------------------------------------------------------------------
// Blob management API
// Copyright (C)2017 Matt Davies, all rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>

#if !OS_WIN32
#   error Not implemented for your OS.
#endif

//----------------------------------------------------------------------------------------------------------------------
// Blob API
// A blob is a binary object that lies in memory.  We use file-mapping to fetch the data from a file.
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    u8*     bytes;
    i64     size;

#if OS_WIN32
    HANDLE  file;
    HANDLE  fileMap;
#endif
}
Blob;

Blob blobLoad(const char* fileName);
void blobUnload(Blob data);

Blob blobMake(const char* fileName, i64 size);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

#if OS_WIN32

Blob blobLoad(const char* fileName)
{
    Blob b = { 0 };

    b.file = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (b.file)
    {
        DWORD fileSizeHigh, fileSizeLow;
        fileSizeLow = GetFileSize(b.file, &fileSizeHigh);
        b.fileMap = CreateFileMappingA(b.file, 0, PAGE_READONLY, fileSizeHigh, fileSizeLow, 0);

        if (b.fileMap)
        {
            b.bytes = MapViewOfFile(b.fileMap, FILE_MAP_READ, 0, 0, 0);
            b.size = ((i64)fileSizeHigh << 32) | fileSizeLow;
        }
        else
        {
            blobUnload(b);
        }
    }

    return b;
}

void blobUnload(Blob b)
{
    if (b.bytes)        UnmapViewOfFile(b.bytes);
    if (b.fileMap)      CloseHandle(b.fileMap);
    if (b.file)         CloseHandle(b.file);

    b.bytes = 0;
    b.size = 0;
    b.file = INVALID_HANDLE_VALUE;
    b.fileMap = INVALID_HANDLE_VALUE;
}

Blob blobMake(const char* fileName, i64 size)
{
    Blob b = { 0 };

    b.file = CreateFileA(fileName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (b.file)
    {
        DWORD fileSizeLow = (size & 0xffffffff);
        DWORD fileSizeHigh = (size >> 32);
        b.fileMap = CreateFileMappingA(b.file, 0, PAGE_READWRITE, fileSizeHigh, fileSizeLow, 0);

        if (b.fileMap)
        {
            b.bytes = MapViewOfFile(b.fileMap, FILE_MAP_WRITE, 0, 0, 0);
            b.size = size;
        }
        else
        {
            blobUnload(b);
        }
    }

    return b;
}

#endif

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
