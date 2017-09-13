//----------------------------------------------------------------------------------------------------------------------
// Memory API
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>

#ifndef K_ARENA_INCREMENT
#   define K_ARENA_INCREMENT   4096
#endif

#ifndef K_ARENA_ALIGN
#   define K_ARENA_ALIGN       8
#endif

//----------------------------------------------------------------------------------------------------------------------
// Basic allocation
//----------------------------------------------------------------------------------------------------------------------

void* memoryAlloc(i64 numBytes, const char* file, int line);
void* memoryRealloc(void* address, i64 oldNumBytes, i64 newNumBytes, const char* file, int line);
void memoryFree(void* address, i64 numBytes, const char* file, int line);

void memoryCopy(const void* src, void* dst, i64 numBytes);
void memoryMove(const void* src, void* dst, i64 numBytes);
int memoryCompare(const void* mem1, const void* mem2, i64 numBytes);
void memoryClear(void* mem, i64 numBytes);

#define K_ALLOC(numBytes) memoryAlloc((numBytes), __FILE__, __LINE__)
#define K_REALLOC(address, oldNumBytes, newNumBytes) memoryRealloc((address), (oldNumBytes), (newNumBytes), __FILE__, __LINE__)
#define K_FREE(address, oldNumBytes) memoryFree((address), (oldNumBytes), __FILE__, __LINE__)

//----------------------------------------------------------------------------------------------------------------------
// Arena allocator
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    u8*     start;
    u8*     end;
    i64     cursor;
    i64     restore;
}
Arena;

// Create a new Arena.
void arenaInit(Arena* arena, i64 initialSize);

// Deallocate the memory used by the arena.
void arenaDone(Arena* arena);

// Allocate some memory on the arena.
void* arenaAlloc(Arena* arena, i64 numytes);

// Ensure that the next allocation is aligned to 16 bytes boundary.
void* arenaAlign(Arena* arena);

// Combine alignment and allocation into one function for convenience.
void* arenaAlignedAlloc(Arena* arena, i64 numBytes);

// Create a restore point so that any future allocations can be deallocated in one go.
void arenaPush(Arena* arena);

// Deallocate memory from the previous restore point.
void arenaPop(Arena* arena);

#define K_ARENA_ALLOC(arena, t, count) (t *)arenaAlignedAlloc((arena), sizeof(t) * (count))

//----------------------------------------------------------------------------------------------------------------------
// Arrays
//----------------------------------------------------------------------------------------------------------------------

#define Array(T) T*

#define K_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Destroy an array
#define arrayRelease(a) ((a) ? K_FREE((u8 *)a - (sizeof(i64) * 2), (sizeof(*a) * __arrayCapacity(a)) + (sizeof(i64) * 2)), 0 : 0)

// Add an element to the end of an array
#define arrayAdd(a, v) (__arrayMayGrow(a, 1), (a)[__arrayCount(a)++] = (v))

// Return the number of elements in an array
#define arrayCount(a) ((a) ? __arrayCount(a) : 0)

// Add n uninitialised elements to the array
#define arrayExpand(a, n) (__arrayMayGrow(a, n), __arrayCount(a) += (n), &(a)[__arrayCount(a) - (n)])

// Reserve capacity for n extra items to the array
#define arrayReserve(a, n) (__arrayMayGrow(a, n))

// Clear the array
#define arrayClear(a) (arrayCount(a) = 0)

// Delete an array entry
#define arrayDelete(a, i) (memoryMove(&(a)[(i)+1], &(a)[(i)], (__arrayCount(a) - (i) - 1) * sizeof(*a)), --__arrayCount(a), (a))

//
// Internal routines
//

#define __arrayRaw(a) ((i64 *)(a) - 2)
#define __arrayCount(a) __arrayRaw(a)[1]
#define __arrayCapacity(a) __arrayRaw(a)[0]

#define __arrayNeedsToGrow(a, n) ((a) == 0 || __arrayCount(a) + (n) >= __arrayCapacity(a))
#define __arrayMayGrow(a, n) (__arrayNeedsToGrow(a, (n)) ? __arrayGrow(a, n) : 0)
#define __arrayGrow(a, n) ((a) = __arrayInternalGrow((a), (n), sizeof(*(a))))

internal void* __arrayInternalGrow(void* a, i64 increment, i64 elemSize);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

//----------------------------------------------------------------------------------------------------------------------
// Basic allocation
//----------------------------------------------------------------------------------------------------------------------

internal void* memoryOp(void* oldAddress, i64 oldNumBytes, i64 newNumBytes, const char* file, int line)
{
    void* p = 0;

    if (newNumBytes)
    {
        p = realloc(oldAddress, newNumBytes);
    }
    else
    {
        free(oldAddress);
    }

    return p;
}

void* memoryAlloc(i64 numBytes, const char* file, int line)
{
    return memoryOp(0, 0, numBytes, file, line);
}

void* memoryRealloc(void* address, i64 oldNumBytes, i64 newNumBytes, const char* file, int line)
{
    return memoryOp(address, oldNumBytes, newNumBytes, file, line);
}

void memoryFree(void* address, i64 numBytes, const char* file, int line)
{
    memoryOp(address, numBytes, 0, file, line);
}

void memoryCopy(const void* src, void* dst, i64 numBytes)
{
    memcpy(dst, src, (size_t)numBytes);
}

void memoryMove(const void* src, void* dst, i64 numBytes)
{
    memmove(dst, src, (size_t)numBytes);
}

int memoryCompare(const void* mem1, const void* mem2, i64 numBytes)
{
    return memcmp(mem1, mem2, (size_t)numBytes);
}

void memoryClear(void* mem, i64 numBytes)
{
    memset(mem, 0, (size_t)numBytes);
}

//----------------------------------------------------------------------------------------------------------------------
// Arena control
//----------------------------------------------------------------------------------------------------------------------

void arenaInit(Arena* arena, i64 initialSize)
{
    u8* buffer = (u8 *)malloc(initialSize);
    if (buffer)
    {
        arena->start = buffer;
        arena->end = arena->start + initialSize;
        arena->cursor = 0;
        arena->restore = -1;
    }
}

void arenaDone(Arena* arena)
{
    free(arena->start);
    arena->start = 0;
    arena->end = 0;
    arena->cursor = 0;
    arena->restore = -1;
}

void* arenaAlloc(Arena* arena, i64 size)
{
    void* p = 0;
    if ((arena->start + arena->cursor + size) > arena->end)
    {
        // We don't have enough room
        i64 currentSize = (i64)(arena->end - (u8 *)arena);
        i64 requiredSize = currentSize + size;
        i64 newSize = currentSize + K_MAX(requiredSize, K_ARENA_INCREMENT);

        u8* newArena = (u8 *)realloc(arena, newSize);

        if (newArena)
        {
            arena->start = newArena;
            arena->end = newArena + newSize;

            // Try again!
            p = arenaAlloc(arena, size);
        }
    }
    else
    {
        p = arena->start + arena->cursor;
        arena->cursor += size;
    }

    return p;
}

void* arenaAlign(Arena* arena)
{
    i64 mod = arena->cursor % K_ARENA_ALIGN;
    void* p = arena->start + arena->cursor;

    if (mod)
    {
        // We need to align
        arenaAlloc(arena, K_ARENA_ALIGN - mod);
    }

    return p;
}

void* arenaAlignedAlloc(Arena* arena, i64 numBytes)
{
    arenaAlign(arena);
    return arenaAlloc(arena, numBytes);
}

void arenaPush(Arena* arena)
{
    arenaAlign(arena);
    {
        i64* p = arenaAlloc(arena, sizeof(i64) * 2);
        p[0] = 0xaaaaaaaaaaaaaaaa;
        p[1] = arena->restore;
        arena->restore = (i64)((u8 *)p - arena->start);
    }
}

void arenaPop(Arena* arena)
{
    i64* p = 0;
    K_ASSERT(arena->restore != -1, "Make sure we have some restore points left");
    arena->cursor = arena->restore;
    p = (i64 *)(arena->start + arena->cursor);
    p[0] = 0xbbbbbbbbbbbbbbbb;
    arena->restore = p[1];
}

//----------------------------------------------------------------------------------------------------------------------
// Arrays
//----------------------------------------------------------------------------------------------------------------------

internal void* __arrayInternalGrow(void* a, i64 increment, i64 elemSize)
{
    i64 doubleCurrent = a ? 2 * __arrayCapacity(a) : 0;
    i64 minNeeded = arrayCount(a) + increment;
    i64 capacity = doubleCurrent > minNeeded ? doubleCurrent : minNeeded;
    i64 oldBytes = a ? elemSize * arrayCount(a) + sizeof(i64) * 2 : 0;
    i64 bytes = elemSize * capacity + sizeof(i64) * 2;
    i64* p = (i64 *)K_REALLOC(a ? __arrayRaw(a) : 0, oldBytes, bytes);
    if (p)
    {
        if (!a) p[1] = 0;
        p[0] = capacity;
        return p + 2;
    }
    else
    {
        return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif
