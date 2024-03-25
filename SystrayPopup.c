#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#define WINDOW_WIDTH 100
#define WINDOW_HEIGHT 100

#define messageboxf(title, fmt, ...) do {           \
    char buf[300];                                  \
    snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
    MessageBoxA(NULL, buf, title, MB_OK);           \
} while(0)

static DWORD start;
static FILE *logfile = NULL;

#define logf(fmt,...) do {                                            \
    fprintf(stderr, "%u|%u| " fmt "\n", GetTickCount() - start, GetCurrentThreadId(), ##__VA_ARGS__); \
    fflush(stderr);                                                   \
    fprintf(logfile, "%u|%u| " fmt "\n", GetTickCount() - start, GetCurrentThreadId(), ##__VA_ARGS__); \
    fflush(logfile);                                                  \
} while (0)

#define fatal(fmt, ...) do {                        \
    messageboxf("Fatal Error", fmt, ##__VA_ARGS__); \
    abort();                                        \
} while(0)

#define WM_USER_SHELL_NOTIFY WM_USER
#define NOTIFY_ICON_ID 0
#define IDC_QUIT 8

static void update_text(HWND textwnd, HWINEVENTHOOK hook)
{
    const WCHAR *text = hook ? L"Background" : L"Foreground";
    LRESULT result = SendMessageW(textwnd, WM_SETTEXT, 0, (LPARAM)text);
    if (result == 0)
        fatal("WM_SETTEXT failed, result=%lld", result);
}

static HWND global_hwnd = NULL;
static HWND global_textwnd = NULL;
static HWINEVENTHOOK global_winevent_hook = NULL;

static void on_win_event(
  HWINEVENTHOOK hWinEventHook,
  DWORD event,
  HWND hwnd,
  LONG idObject,
  LONG idChild,
  DWORD idEventThread,
  DWORD dwmsEventTime
) {
    if (hWinEventHook != global_winevent_hook) abort();
    if (event != EVENT_SYSTEM_FOREGROUND) abort();

    BOOL is_our_window = (hwnd == global_hwnd);
    if (is_our_window) {
        // I've never seen this happen, if it did happen
        // we should be able to uninstall our hook because
        // now we'll get the proper window message when the
        // user clicks away from our window
        logf("Ignoring WinEvent cause it's our window");
        // abort left in just so I can know if this every happens
        abort();
    } else {
        logf(
            "WinEventHook hwnd=0x%zx id=%d child=%d thread=%u",
            (size_t)hwnd,
            is_our_window,
            idObject,
            idChild,
            idEventThread
        );
        // TODO: is there an API to deactivate the window instead?
        BOOL was_visible = ShowWindow(global_hwnd, SW_HIDE);
        logf("hide window from winevent hook (was_visible=%d)", !!was_visible);
    }

    if (!UnhookWinEvent(hWinEventHook))
        fatal("UnhookWinEvent failed, error=%u", GetLastError());
    global_winevent_hook = NULL;
    update_text(global_textwnd, global_winevent_hook);
}

static void show_window(HWND hWnd, POINT pt)
{
    RECT window_rect;
    SIZE size_arg = { WINDOW_WIDTH, WINDOW_HEIGHT };
    if (TRUE != CalculatePopupWindowPosition(
        &pt,
        &size_arg,
        TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_WORKAREA,
        NULL,
        &window_rect
    ))
        fatal("CalculatePopupWindowPosition failed, error=%d", GetLastError());

    logf(
        "set window pos %d,%d (point %u,%u)",
        window_rect.left, window_rect.top,
        pt.x, pt.y
    );
    if (TRUE != SetWindowPos(
        hWnd,
        HWND_TOPMOST,
        window_rect.left, window_rect.top,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        0
    ))
        fatal("SetWindowPos failed, error=%d", GetLastError());

    if (!SetForegroundWindow(hWnd)) {
        // If SetForegroundWindow fails, then it means the OS won't curently
        // allow us to take the input focus.  This is important for the popup window
        // because without input focus, we won't receive any messages that tell us
        // when the user has clicked away from our window so we can know to hide it again.
        //
        // As a fallback, we install a "WinEventHook" that tells us whenever the system foreground
        // window changes, and we can then close the window if another window comes to the
        // foreground.  This isn't perfect but it seems to be the best solution thus far.
        logf("SetForegroundWindow failed");

        if (!global_winevent_hook) {
            global_winevent_hook = SetWinEventHook(
                EVENT_SYSTEM_FOREGROUND,
                EVENT_SYSTEM_FOREGROUND,
                NULL,
                on_win_event, // callback function
                0, // all processes
                0, // all threads
                WINEVENT_OUTOFCONTEXT
            );
            if (!global_winevent_hook)
                fatal("SetWinEventHook failed, error=%u", GetLastError());
        }
    } else {
        if (global_winevent_hook) {
            if (!UnhookWinEvent(global_winevent_hook))
                fatal("UnhookWinEvent failed, error=%u", GetLastError());
            global_winevent_hook = NULL;
        }
    }
    update_text(global_textwnd, global_winevent_hook);

    BOOL was_visible = ShowWindow(hWnd, SW_SHOW);
    logf("show window (was_visible=%d)", !!was_visible);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (global_winevent_hook) {
        if (hWnd == GetForegroundWindow()) {
            logf("removing winevent hook because we're now the foreground window (msg=%u)", uMsg);
            if (!UnhookWinEvent(global_winevent_hook))
                fatal("UnhookWinEvent failed, error=%u", GetLastError());
            global_winevent_hook = NULL;
            update_text(global_textwnd, global_winevent_hook);
        }
    }

    switch (uMsg) {
    case WM_USER_SHELL_NOTIFY: {
        switch (LOWORD(lParam)) {
        case NIN_SELECT:
            logf("NIN_SELECT %d,%d", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            show_window(hWnd, ((POINT){ GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) }));
            break;
        case NIN_KEYSELECT:
            logf("NIN_KEYSELECT %d,%d", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            show_window(hWnd, ((POINT){ GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam) }));
            break;
        case NIN_POPUPOPEN:
            logf("NIN_POPUPOPEN %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case NIN_POPUPCLOSE:
            logf("NIN_POPUPCLOSE %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case WM_CONTEXTMENU:
            logf("ShellNotify: WM_CONTEXTMENU %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case WM_LBUTTONDOWN:
            logf("ShellNotify: WM_LBUTTONDOWN %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case WM_RBUTTONDOWN:
            logf("ShellNotify: WM_RBUTTONDOWN %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case WM_LBUTTONUP:
            logf("ShellNotify: WM_LBUTTONUP %dx%d (ignored)", GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        case WM_MOUSEMOVE:
            // don't log (too many events)
            break;
        default:
            logf("ShellNotify: unhandled msg %u %dx%d", LOWORD(lParam), GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            break;
        }

        return 0;
    }
    case WM_ACTIVATE: {
        if (WA_INACTIVE == LOWORD(wParam)) {
            BOOL was_visible = ShowWindow(hWnd, SW_HIDE);
            logf("hide window (was_visible=%d minimized=%u)", !!was_visible, HIWORD(wParam));
        }
        return 0;
    }
    case WM_COMMAND: {
        if (wParam = IDC_QUIT) {
            logf("Quit button clicked");
            PostQuitMessage(0);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, WCHAR *pCmdLine, int nCmdShow)
{
    start = GetTickCount();
    logfile = fopen("SystrayPopup.log", "w");

    const WCHAR CLASS_NAME[] = L"MainWindow";

    {
        WNDCLASSW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc   = WindowProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = CLASS_NAME;
        if (0 == RegisterClassW(&wc)) {
            MessageBoxW(NULL, L"RegisterClass failed", L"Error", 0);
            return -1;
        }
    }

    global_hwnd = CreateWindowExW(
        WS_EX_TOPMOST|WS_EX_TOOLWINDOW,
        CLASS_NAME,
        L"Popup Window", // Title
        WS_POPUP | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, // Position
        WINDOW_WIDTH, WINDOW_HEIGHT,  // Size
        NULL,       // Parent window
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    if (global_hwnd == NULL)
        fatal("CreateWindow failed, error=%d", GetLastError());

    global_textwnd = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_VISIBLE | WS_CHILD,
        5, 50,
        90, 30,
        global_hwnd,
        NULL,
        NULL,
        NULL
    );
    if (!global_textwnd)
        fatal("CreateWindow for text failed, error=%u", GetLastError());

    if (!CreateWindowExW(
        0,
        L"BUTTON",
        L"Quit",
        WS_VISIBLE | WS_CHILD,
        25, 10,
        50, 30,
        global_hwnd,
        (HMENU)IDC_QUIT,
        NULL,
        NULL
    ))
        fatal("CreateWindow for button failed, error=%d", GetLastError());

    HICON icon = LoadIcon(NULL, IDI_INFORMATION);
    if (!icon) fatal("LoadIcon failed, error=%d", GetLastError());

    {
        NOTIFYICONDATAW data;
        ZeroMemory(&data, sizeof(data));
        data.cbSize = sizeof(data);
        data.hWnd = global_hwnd;
        data.uFlags = NIF_ICON | NIF_MESSAGE;
        data.hIcon = icon;
        data.uCallbackMessage = WM_USER_SHELL_NOTIFY;
        data.uID = NOTIFY_ICON_ID;
        if (!Shell_NotifyIconW(NIM_ADD, &data))
            fatal("ShellNotify failed, error=%d", GetLastError());
    }

    {
        NOTIFYICONDATAW data;
        ZeroMemory(&data, sizeof(data));
        data.cbSize = sizeof(data);
        data.hWnd = global_hwnd;
        data.uVersion = NOTIFYICON_VERSION_4;
        data.uID = NOTIFY_ICON_ID;
        if (TRUE != Shell_NotifyIconW(NIM_SETVERSION, &data))
            fatal("NotifyIcon: SETVERSION failed, error=%u", GetLastError());
    }

    while (1) {
        MSG msg;
        BOOL result = GetMessage(&msg, NULL, 0, 0);
        if (result == -1)
            fatal("GetMessage failed, error=%u", GetLastError());
        if (result == 0) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    {
        NOTIFYICONDATAW data;
        ZeroMemory(&data, sizeof(data));
        data.cbSize = sizeof(data);
        data.hWnd = global_hwnd;
        data.uID = NOTIFY_ICON_ID;
        if (TRUE != Shell_NotifyIconW(NIM_DELETE, &data)) {
            logf("NotifyIcon: DELETE failed, error=%u", GetLastError());
            return -1;
        } else {
            logf("NotifyIcon: deleted");
        }
    }

    return 0;
}
