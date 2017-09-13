//----------------------------------------------------------------------------------------------------------------------
// Process spawning and output capture
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>
#include <kore/k_memory.h>

// Start a process
bool spawnAndWait(const i8* fileName, int argc, const i8** argv);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

bool spawnAndWait(const i8* fileName, int argc, const i8** argv)
{
    bool result = NO;
    char* args;
    char* s;

    // Collect information about the arguments
    size_t fileNameSize = strlen(fileName);
    size_t argsSize = fileNameSize + 1;
    bool* hasSpaces = K_ALLOC(sizeof(bool) * argc);

    for (int i = 0; i < argc; ++i)
    {
        hasSpaces[i] = NO;
        for (int j = 0; argv[i][j] != 0; ++j)
        {
            if (argv[i][j] == ' ') hasSpaces[i] = YES;
            ++argsSize;
        }

        if (hasSpaces[i]) argsSize += 2;    // for quotes
        ++argsSize;                         // following space
    }

    // Generate the command line
    s = args = K_ALLOC(argsSize + 1);
    memoryCopy(fileName, s, fileNameSize);
    s += fileNameSize;
    *s++ = ' ';

    for (int i = 0; i < argc; ++i)
    {
        size_t len = strlen(argv[i]);

        if (hasSpaces[i]) *s++ = '"';
        memoryCopy(argv[i], s, len);
        s += len;
        if (hasSpaces[i]) *s++ = '"';
        if (i < (argc - 1)) *s++ = ' ';
    }
    *s = 0;

#if OS_WIN32
    {
        PROCESS_INFORMATION process;
        STARTUPINFO si;

        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&process, sizeof(process));

        si.cb = sizeof(si);

        if (CreateProcess(NULL, args, 0, 0, 0, NORMAL_PRIORITY_CLASS, 0, 0, &si, &process))
        {
            WaitForSingleObject(process.hProcess, INFINITE);
            result = YES;
        }
        else
        {
            DWORD err = GetLastError();
            err = err;
        }
    }
#else
#   error Implement spawnAndWait for you OS.
#endif

    K_FREE(args, argsSize + 1);
    K_FREE(hasSpaces, sizeof(bool) * argc);

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
