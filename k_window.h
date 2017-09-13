//----------------------------------------------------------------------------------------------------------------------
// Simple bitmap-based window API
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <kore/k_platform.h>
#include <kore/k_memory.h>

typedef int Window;

// Create a window for an image of a certain size and scale it up by a factor.
// The image format must be in BGRA BGRA... format.  However, if you fetch a u32 on a little endian system,
// the u32 will be in format ARGB.
Window windowMake(const char* title, u32* image, int width, int height, int scale);

// Close the window
void windowClose(Window window);

// Pump all window messages.  Returns true while there is a window available.
bool windowPump();

// Use these if you plan to recreate a window without windowPump exiting.
void windowLock();
void windowUnlock();

// Mark window to redraw on next pump.
void windowRedraw(Window window);

// Accessor functions
int windowWidth(Window window);
int windowHeight(Window window);
u32* windowImage(Window window);

// Create a console window for debug output
void windowConsole();

// Wait for a key in the console window
void windowConsolePause();

//----------------------------------------------------------------------------------------------------------------------
// Window Events

typedef void(*WindowKeyDownEvent) (Window wnd, u32 vkCode);
typedef void(*WindowKeyUpEvent) (Window wnd, u32 vkCode);
typedef void(*WindowCharEvent) (Window wnd, char ch);

void windowHandleKeyDownEvent(Window window, WindowKeyDownEvent handler);
void windowHandleKeyUpEvent(Window window, WindowKeyUpEvent handler);
void windowHandleCharEvent(Window window, WindowCharEvent handler);

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef K_IMPLEMENTATION

#include <conio.h>
#include <fcntl.h>
#include <io.h>

typedef struct
{
#if OS_WIN32
    HWND handle;
    BITMAPINFO info;
#endif
    u32* image;
    int imgWidth;
    int imgHeight;
    int wndWidth;
    int wndHeight;

    WindowKeyDownEvent evKeyDown;
    WindowKeyUpEvent evKeyUp;
    WindowCharEvent evChar;
}
WindowInfo;

Array(WindowInfo) gWindows = 0;
int gWindowCount = 0;
#if OS_WIN32
ATOM gWindowClassAtom = 0;
#endif

internal Window __windowAllocHandle()
{
    for (int i = 0; i < arrayCount(gWindows); ++i)
    {
        if (gWindows[i].handle == 0)
        {
            return i;
        }
    }

    gWindows = arrayExpand(gWindows, 1);
    return (Window)(arrayCount(gWindows) - 1);
}


#if OS_WIN32
internal Window __windowFindHandle(HWND wnd)
{
    i64 count = arrayCount(gWindows);
    for (int i = 0; i < count; ++i)
    {
        if (gWindows[i].handle == wnd) return i;
    }

    return -1;
}

typedef struct WindowCreateInfo
{
    Window handle;
}
WindowCreateInfo;

internal LRESULT CALLBACK __windowProc(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == WM_CREATE)
    {
        CREATESTRUCTA* cs = (CREATESTRUCTA *)l;
        WindowCreateInfo* wci = (WindowCreateInfo *)cs->lpCreateParams;
        gWindows[wci->handle].handle = wnd;
    }
    else
    {
        Window window = __windowFindHandle(wnd);
        WindowInfo* info = (window == -1 ? 0 : &gWindows[window]);

        switch (msg)
        {
        case WM_SIZE:
            if (info)
            {
                int bitmapMemorySize = 0;

                info->wndWidth = LOWORD(l);
                info->wndHeight = HIWORD(l);

                ZeroMemory(&info->info.bmiHeader, sizeof(info->info.bmiHeader));

                info->info.bmiHeader.biSize = sizeof(info->info.bmiHeader);
                info->info.bmiHeader.biWidth = info->imgWidth;
                info->info.bmiHeader.biHeight = -info->imgHeight;
                info->info.bmiHeader.biPlanes = 1;
                info->info.bmiHeader.biBitCount = 32;
                info->info.bmiHeader.biClrImportant = BI_RGB;
            }
            break;

        case WM_PAINT:
            if (info)
            {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(wnd, &ps);
                StretchDIBits(dc,
                    0, 0, info->wndWidth, info->wndHeight,
                    0, 0, info->imgWidth, info->imgHeight,
                    info->image, &info->info,
                    DIB_RGB_COLORS, SRCCOPY);
                EndPaint(wnd, &ps);
            }
            break;

        case WM_CLOSE:
            DestroyWindow(wnd);
            break;

        case WM_DESTROY:
            if (--gWindowCount == 0)
            {
                PostQuitMessage(0);
            }
            info->handle = 0;
            break;

        case WM_KEYDOWN:
            if (info->evKeyDown) info->evKeyDown(window, (u32)w);
            break;

        case WM_KEYUP:
            if (info->evKeyUp) info->evKeyDown(window, (u32)w);
            break;

        case WM_CHAR:
            if (info->evChar) info->evChar(window, (char)w);
            break;

        default:
            return DefWindowProcA(wnd, msg, w, l);
        }
    }

    return 0;
}
#endif // OS_WIN32

Window windowMake(const char* title, u32* image, int width, int height, int scale)
{
    WindowCreateInfo wci;
    Window w = __windowAllocHandle();

    memoryClear(&gWindows[w], sizeof(WindowInfo));

    wci.handle = w;

    gWindows[w].handle = 0;
    gWindows[w].image = image;
    gWindows[w].imgWidth = width;
    gWindows[w].imgHeight = height;
    gWindows[w].wndWidth = width * scale;
    gWindows[w].wndHeight = width * scale;

#if OS_WIN32
    RECT r = { 0, 0, width * scale, height * scale };
    int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;

    if (!gWindowClassAtom)
    {
        WNDCLASSEXA wc = { 0 };

        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &__windowProc;
        wc.hInstance = GetModuleHandleA(0);
        wc.hIcon = wc.hIconSm = LoadIconA(0, IDI_APPLICATION);
        wc.hCursor = LoadCursorA(0, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "k_bitmap_window";

        gWindowClassAtom = RegisterClassExA(&wc);
    }

    AdjustWindowRect(&r, style, FALSE);

    gWindows[w].handle = CreateWindowA("k_bitmap_window", title, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        0, 0, GetModuleHandleA(0), &wci);
#else
#   error Implement window creation for your platform
#endif

    ++gWindowCount;

    return w;
}

void windowClose(Window window)
{
#if OS_WIN32
    SendMessageA(gWindows[window].handle, WM_CLOSE, 0, 0);
#endif
}

bool windowPump()
{
    bool cont = YES;

#if OS_WIN32
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE))
    {
        if (!GetMessageA(&msg, 0, 0, 0)) cont = NO;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
#else
#   error Write message pump for your OS
#endif

    return cont;
}

void windowLock()
{
    ++gWindowCount;
}

void windowUnlock()
{
    --gWindowCount;
}

void windowRedraw(Window window)
{
#if OS_WIN32
    InvalidateRect(gWindows[window].handle, 0, FALSE);
#else
#   error Implement windowRedraw for your OS
#endif
}

int windowWidth(Window window)
{
    return gWindows[window].imgWidth;
}

int windowHeight(Window window)
{
    return gWindows[window].imgHeight;
}

u32* windowImage(Window window)
{
    return gWindows[window].image;
}

void windowEnableANSIColours()
{
    DWORD mode;
    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(handle_out, &mode);
    mode |= 0x4;
    SetConsoleMode(handle_out, mode);
}

void windowConsole()
{
    AllocConsole();
    SetConsoleTitleA("Debug Window");

    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((intptr_t)handle_out, _O_TEXT);
    FILE* hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    freopen("CONOUT$", "w", stdout);

    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((intptr_t)handle_in, _O_TEXT);
    FILE* hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 0);
    freopen("CONIN$", "r", stdin);

    windowEnableANSIColours();
}

void windowConsolePause()
{
    printf("\n\033[33;1mPress any key...\033[0m\n\n");
    _getch();
}

void windowHandleKeyDownEvent(Window window, WindowKeyDownEvent handler)
{
    gWindows[window].evKeyDown = handler;
}

void windowHandleKeyUpEvent(Window window, WindowKeyUpEvent handler)
{
    gWindows[window].evKeyUp = handler;
}

void windowHandleCharEvent(Window window, WindowCharEvent handler)
{
    gWindows[window].evChar = handler;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // K_IMPLEMENTATION
