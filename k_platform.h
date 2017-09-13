//----------------------------------------------------------------------------------------------------------------------CPU
// Platform API
// Includes standard headers for the platform, defines some macros and type definitions
//
// Copyright (C) Matt Davies, all rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#define YES (1)
#define NO (0)

// Compiler defines
#define COMPILER_MSVC       NO

// OS defines
#define OS_WIN32            NO

// CPU defines
#define CPU_X86             NO
#define CPU_X64             NO

//----------------------------------------------------------------------------------------------------------------------
// Compiler determination

#ifdef _MSC_VER
#   undef COMPILER_MSVC
#   define COMPILER_MSVC YES
#else
#   error Unknown compiler.  Please define COMPILE_XXX macro for your compiler.
#endif

//----------------------------------------------------------------------------------------------------------------------
// OS determination

#ifdef _WIN32
#   undef OS_WIN32
#   define OS_WIN32 YES
#else
#   error Unknown OS.  Please define OS_XXX macro for your operating system.
#endif

//----------------------------------------------------------------------------------------------------------------------
// CPU determination

#if COMPILER_MSVC
#   if defined(_M_X64)
#       undef CPU_X64
#       define CPU_X64 YES
#   elif defined(_M_IX86)
#       undef CPU_X86
#       define CPU_X86 YES
#   else
#       error Can not determine processor - something's gone very wronge here!
#   endif
#else
#   error Add CPU determination code for your compiler.
#endif

//----------------------------------------------------------------------------------------------------------------------
// Standard headers

#if OS_WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

//----------------------------------------------------------------------------------------------------------------------
// Basic types and defintions

#if OS_WIN32
typedef INT8    i8;
typedef INT16   i16;
typedef INT32   i32;
typedef INT64   i64;

typedef UINT8   u8;
typedef UINT16  u16;
typedef UINT32  u32;
typedef UINT64  u64;

typedef float   f32;
typedef double  f64;

typedef char    bool;

#else
#   error Define basic types for your platform.
#endif

#define YES (1)
#define NO (0)
#define K_BOOL(b) ((b) ? YES : NO)

#define KB(x) (1024 * (x))
#define MB(x) (1024 * KB(x))
#define GB(x) (1024 * MB(x))

#define K_MIN(a, b) ((a) < (b) ? (a) : (b))
#define K_MAX(a, b) ((a) < (b) ? (b) : (a))
#define K_ABS(a) ((a) < 0 ? -(a) : (a))

#define K_ASSERT(x, ...) assert(x)

#define internal static

//----------------------------------------------------------------------------------------------------------------------
// Timer functions

#if OS_WIN32
typedef struct
{
    LARGE_INTEGER freq;
    LARGE_INTEGER time;
}
Time;
#else
#   error Define Time for your platform!
#endif

// Start the timer
void timerStart(Time* time);

// Return the elapsed time in seconds
f64 timerEnd(Time* time);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

#if OS_WIN32

void timerStart(Time* time)
{
    QueryPerformanceFrequency(&time->freq);
    QueryPerformanceCounter(&time->time);
}

f64 timerEnd(Time* time)
{
    LARGE_INTEGER stopTime;
    LARGE_INTEGER elapsedTime;
    f64 t;

    QueryPerformanceCounter(&stopTime);
    elapsedTime.QuadPart = (stopTime.QuadPart - time->time.QuadPart);

    t = (f64)elapsedTime.QuadPart / (f64)time->freq.QuadPart;
    return t;
}

#endif

extern int kmain(int argc, char** argv);

int main(int argc, char** argv)
{
    return kmain(argc, argv);
}

#if OS_WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prevInst, LPSTR cmdLine, int cmdShow)
{
    return kmain(__argc, __argv);
}
#endif

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
