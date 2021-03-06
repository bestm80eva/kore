//----------------------------------------------------------------------------------------------------------------------
// String API
// Copyright (C)2017 Matt Davies, all rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_memory.h>

typedef i8* String;

//----------------------------------------------------------------------------------------------------------------------
// Hashing
// Based on FNV-1a
//----------------------------------------------------------------------------------------------------------------------

u64 hash(const u8* buffer, i64 len);

//----------------------------------------------------------------------------------------------------------------------
// Dynamic strings
//----------------------------------------------------------------------------------------------------------------------

// Create a string.
String stringMake(const i8* str);

// Create a string from a byte range determined by a start and finish.
String stringMakeRange(const i8* start, const i8* end);

// Release a string back to memory.
void stringRelease(String str);

// Create a new string by appending it to another.
String stringAppend(String str1, String str2);
String stringAppendCStr(String str1, const i8* str2);

// Attributes of the string
i64 stringSize(String str);
u64 stringHash(String str);

// String formatting
String stringFormat(const i8* format, ...);
String stringFormatArgs(const i8* format, va_list args);

//----------------------------------------------------------------------------------------------------------------------
// Arena static strings
// These are strings that are allocated on an arena and are just used for construction.  No operations are intended
// to be run on them after creation.
//----------------------------------------------------------------------------------------------------------------------

// Constructs a static string on the arena.
String stringArenaCopy(Arena* arena, const i8* str);

// Constructs a static string on the arena with a range.
String stringArenaCopyRange(Arena* arena, const i8* str, const i8* end);

// Create a formatted string on the arena with va_list args.
String stringArenaFormatArgs(Arena* arena, const i8* format, va_list args);

// Create formatted string on the arena with variable arguments.
String stringArenaFormat(Arena* arena, const i8* format, ...);

//----------------------------------------------------------------------------------------------------------------------
// String tables
//
// The string table comprises of a header, a hash table, and an area for string storage.
//
// The header comprises of a single i64 which is the size of the hash table.
//
// Next the hash table is an array of N pointers to the beginning of a linked list.  THe index of the hash table is
// generated by the mod of the string's hash and the size of the table.
//
// The string storage is just string information appended one after the other.  For each string you have this info:
//
// Offset   Size    Description
//  0       8       Link to next string with the same hash index, indexed from beginning of string table
//  8       8       64-bit hash of string (not including null terminator)
//  16      ?       Actual string followed by a null terminator.
//
// Each string block is aligned in memory.  No string is allowed to have a hash of zero.  A string token of 0 is
// always a null string.
//----------------------------------------------------------------------------------------------------------------------

typedef Arena StringTable;
typedef const i8* StringToken;

// Initialise a new string table
void stringTableInit(StringTable* table, i64 size, i64 sizeHashTableSize);

// Destroy the string table
void stringTableDone(StringTable* table);

// Add a string to the table and return a unique token representing it.  If it already exists, its token will be
// returned.
StringToken stringTableAdd(StringTable* table, const i8* str);

// Add a string view to the table.
StringToken stringTableAddRange(StringTable* table, const i8* str, const i8* end);

//----------------------------------------------------------------------------------------------------------------------
// Path strings
//
// Given the path "c:\dir1\dir2\file.foo.ext"
//
//  Path        "c:\dir1\dir2\file.foo.ext"
//  Directory   "c:\dir1\dir2"
//  Filename    "file.foo.ext"
//  Extension   "ext"
//  Base        "file.foo"
//
//----------------------------------------------------------------------------------------------------------------------

// Return the directory of a path
String pathDirectory(String path);

// Return a new string with the extension removed
String pathRemoveExtension(String path);

// Return a new string with the extension of the path changed to a new one.
String pathReplaceExtension(String path, const char* ext);

// Join two paths together, the second must be a relative path.
String pathJoin(String p1, String p2);

//----------------------------------------------------------------------------------------------------------------------
// String utilities
//
// stringCompareStringRange     Compare s1..s2 with null terminated s3.
//----------------------------------------------------------------------------------------------------------------------

int stringCompareStringRange(const i8* s1, const i8* s2, const i8* s3);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

//----------------------------------------------------------------------------------------------------------------------
// Hashing
//----------------------------------------------------------------------------------------------------------------------

u64 hash(const u8* buffer, i64 len)
{
    u64 h = 14695981039346656037;
    for (i64 i = 0; i < len; ++i)
    {
        h ^= *buffer++;
        h *= (u64)1099511628211ull;
    }

    return h;
}

u64 hashString(const i8* str)
{
    u64 h = 14695981039346656037;
    while (*str != 0)
    {
        h ^= *str++;
        h *= (u64)1099511628211ull;
    }

    return h;
}

//----------------------------------------------------------------------------------------------------------------------
// Strings
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    i64     size;
    i64     capacity;
    u64     hash;
    i32     refCount;
    i32     magic;
    i8      str[0];
}
StringHeader;

#define K_STRING_HEADER(str) ((StringHeader *)(str) - 1)
#define K_CHECK_STRING(str) K_ASSERT(K_STRING_HEADER(str)->magic == 0xc0deface, "Not valid String (maybe a i8*)!")

internal StringHeader* stringAlloc(i64 size)
{
    StringHeader* hdr = 0;

    hdr = K_ALLOC(sizeof(StringHeader) + size + 1);
    if (hdr)
    {
        hdr->size = size;
        hdr->capacity = size + 1;
        hdr->hash = 0;
        hdr->refCount = 1;
        hdr->magic = 0xc0deface;
        hdr->str[size] = 0;
    }

    return hdr;
}

String stringMake(const i8* str)
{
    i64 size = (i64)strlen(str);
    return stringMakeRange(str, str + size);
}

String stringMakeRange(const i8* start, const i8* end)
{
    i64 size = (i64)(end - start);
    StringHeader* hdr = stringAlloc(size);
    if (hdr)
    {
        memoryCopy(start, hdr->str, size);
        hdr->hash = hash(hdr->str, size);
    }
    return hdr ? hdr->str : 0;
}

void stringRelease(String str)
{
    StringHeader* hdr = K_STRING_HEADER(str);
    K_CHECK_STRING(str);
    if (--hdr->refCount == 0)
    {
        K_FREE(hdr, sizeof(StringHeader) + hdr->capacity);
    }
}

String stringAppend(String str1, String str2)
{
    K_CHECK_STRING(str1);
    K_CHECK_STRING(str2);

    {
        i64 sizeStr1 = stringSize(str1);
        i64 sizeStr2 = stringSize(str2);
        i64 totalSize = sizeStr1 + sizeStr2;
        StringHeader* s = stringAlloc(totalSize);

        if (s)
        {
            memoryCopy(str1, s->str, sizeStr1);
            memoryCopy(str2, s->str + sizeStr1, sizeStr2);
            s->hash = hash(s->str, totalSize);
        }

        return s ? s->str : 0;
    }
}

String stringAppendCStr(String str1, const i8* str2)
{
    K_CHECK_STRING(str1);

    {
        i64 sizeStr1 = stringSize(str1);
        i64 sizeStr2 = (i64)strlen(str2);
        i64 totalSize = sizeStr1 + sizeStr2;
        StringHeader* s = stringAlloc(totalSize);

        if (s)
        {
            memoryCopy(str1, s->str, sizeStr1);
            memoryCopy(str2, s->str + sizeStr1, sizeStr2);
            s->hash = hash(s->str, totalSize);
        }

        return s ? s->str : 0;
    }
}

i64 stringSize(String str)
{
    K_CHECK_STRING(str);
    return str ? K_STRING_HEADER(str)->size : 0;
}

u64 stringHash(String str)
{
    K_CHECK_STRING(str);
    return str ? K_STRING_HEADER(str)->hash : 0;
}

String stringFormatArgs(const i8* format, va_list args)
{
    int numChars = vsnprintf(0, 0, format, args);
    StringHeader* hdr = stringAlloc(numChars);
    vsnprintf(hdr->str, numChars + 1, format, args);
    hdr->hash = hashString(hdr->str);
    return hdr->str;
}

String stringFormat(const i8* format, ...)
{
    String str;
    va_list args;
    va_start(args, format);
    str = stringFormatArgs(format, args);
    va_end(args);
    return str;
}

//----------------------------------------------------------------------------------------------------------------------
// Static Arena Strings
//----------------------------------------------------------------------------------------------------------------------

String stringArenaCopy(Arena* arena, const i8* str)
{
    i64 len = (i64)(strlen(str));
    return stringArenaCopyRange(arena, str, str + len);
}

String stringArenaCopyRange(Arena* arena, const i8* str, const i8* end)
{
    i64 len = (i64)(end - str);
    i8* buffer = (i8 *)arenaAlignedAlloc(arena, sizeof(StringHeader) + len + 1);
    StringHeader* hdr = (StringHeader *)buffer;

    buffer += sizeof(StringHeader);
    hdr->size = len;
    hdr->capacity = -1;
    hdr->hash = 0;
    hdr->refCount = -1;
    hdr->magic = 0xc0deface;

    memoryCopy(str, buffer, len);
    buffer[len] = 0;

    return buffer;
}

String stringArenaFormatArgs(Arena* arena, const i8* format, va_list args)
{
    int numChars = vsnprintf(0, 0, format, args);
    i8* buffer = (i8 *)arenaAlignedAlloc(arena, sizeof(StringHeader) + numChars + 1);
    StringHeader* hdr = (StringHeader*)buffer;

    buffer += sizeof(StringHeader);
    vsnprintf(buffer, numChars + 1, format, args);
    hdr->size = numChars;
    hdr->capacity = -1;
    hdr->hash = 0;
    hdr->refCount = -1;
    hdr->magic = 0xc0deface;

    return buffer;
}

String stringArenaFormat(Arena* arena, const i8* format, ...)
{
    va_list args;
    String s = 0;

    va_start(args, format);
    s = stringArenaFormatArgs(arena, format, args);
    va_end(args);

    return s;
}

//----------------------------------------------------------------------------------------------------------------------
// String Table
//----------------------------------------------------------------------------------------------------------------------

#define K_STRINGTABLE_HASHTABLE_SIZE(st) (*(i64 *)((st)->start))
#define K_STRINGTABLE_HASHTABLE(st) ((i64 *)((st)->start))

typedef struct
{
    i64     nextBlock;
    i64     length;
    u64     hash;
    char    str[0];
}
StringTableHeader;

void stringTableInit(StringTable* st, i64 size, i64 sizeHashTable)
{
    // Ensure we have enough room for the hash table
    K_ASSERT(size > (i64)(sizeof(i64) * sizeHashTable));
    K_ASSERT(size > 0);
    K_ASSERT(sizeHashTable > 1);

    // Create the memory arena
    arenaInit(st, size);
    if (st->start)
    {
        // Index 0 in the hash table holds the size of the hash table
        i64* hashTable = (i64 *)arenaAlloc(st, sizeHashTable * sizeof(i64));
        K_STRINGTABLE_HASHTABLE_SIZE(st) = sizeHashTable;
        for (int i = 1; i < sizeHashTable; ++i) hashTable[i] = 0;
    }
}

void stringTableDone(StringTable* table)
{
    arenaDone(table);
}

internal const i8* __stringTableAdd(StringTable* table, const i8* str, i64 strLen)
{
    u8* b = (u8 *)table->start;
    i64* hashTable = K_STRINGTABLE_HASHTABLE(table);
    u64 h = hash(str, strLen);
    i64 index = (i64)((u64)h % K_STRINGTABLE_HASHTABLE_SIZE(table));
    i64* blockPtr = 0;

    index = index ? index : 1;
    blockPtr = &hashTable[index];
    while (*blockPtr != 0)
    {
        StringTableHeader* hdr = (StringTableHeader*)&b[*blockPtr];
        if (hdr->hash == h)
        {
            // Possible match
            if (hdr->length == strLen)
            {
                if (memoryCompare(str, hdr->str, strLen) == 0)
                {
                    // Found it!
                    return hdr->str;
                }
            }
        }

        blockPtr = &hdr->nextBlock;
    }

    // The string has not be found - create a new entry
    {
        StringTableHeader* hdr = arenaAlignedAlloc(table, sizeof(StringTableHeader) + strLen + 1);

        if (!hdr) return 0;

        hdr->nextBlock = 0;
        hdr->length = strLen;
        hdr->hash = h;
        memoryCopy(str, hdr->str, strLen);
        hdr->str[strLen] = 0;

        *blockPtr = (u8 *)hdr - b;

        return hdr->str;
    }
}

StringToken stringTableAdd(StringTable* table, const i8* str)
{
    return __stringTableAdd(table, str, (i64)strlen(str));
}

StringToken stringTableAddRange(StringTable* table, const i8* str, const i8* end)
{
    return __stringTableAdd(table, str, (i64)end - (i64)str);
}

//----------------------------------------------------------------------------------------------------------------------
// Paths
//----------------------------------------------------------------------------------------------------------------------

internal const i8* __findLastChar(String path, char c)
{
    K_CHECK_STRING(path);

    {
        const i8* end = path + stringSize(path);
        const i8* s = end;
        while ((*s != c) && (s > path)) --s;
        if (s == path) s = end;

        return s;
    }
}

String pathDirectory(String path)
{
    K_CHECK_STRING(path);

    {
        const i8* endPath = __findLastChar(path, '\\');
        return stringMakeRange(path, endPath);
    }
}

String pathRemoveExtension(String path)
{
    K_CHECK_STRING(path);

    {
        const i8* ext = __findLastChar(path, '.');
        return stringMakeRange(path, ext);
    }
}

String pathReplaceExtension(String path, const char* ext)
{
    K_CHECK_STRING(path);

    {
        const i8* extStart = __findLastChar(path, '.');
        i64 lenExt = (i64)strlen(ext);
        i64 lenBase = (i64)(extStart - path);
        i64 lenStr = lenBase + 1 + lenExt;
        StringHeader* hdr = stringAlloc(lenStr);
        memoryCopy(path, hdr->str, lenBase);
        hdr->str[lenBase] = '.';
        memoryCopy(ext, &hdr->str[lenBase + 1], lenExt);
        return hdr->str;
    }
}

String pathJoin(String p1, String p2)
{
    return stringFormat("%s/%s", p1, p2);
}

//----------------------------------------------------------------------------------------------------------------------
// String utilities
//----------------------------------------------------------------------------------------------------------------------

int stringCompareStringRange(const i8* s1, const i8* s2, const i8* s3)
{
    while ((s1 < s2) && *s3 != 0)
    {
        int d = *s1++ - *s3++;
        if (d != 0) return d;
    }

    return s1 == s2 ? -*s3 : *s1 - *s3;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
