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
    fprintf(stderr, "%u: " fmt "\n", GetTickCount() - start,##__VA_ARGS__);   \
    fflush(stderr);                                                   \
    fprintf(logfile, "%u: " fmt "\n", GetTickCount() - start, ##__VA_ARGS__); \
    fflush(logfile);                                                  \
} while (0)

#define fatal(fmt, ...) do {                        \
    messageboxf("Fatal Error", fmt, ##__VA_ARGS__); \
    abort();                                        \
} while(0)

#define WM_USER_SHELL_NOTIFY WM_USER
#define NOTIFY_ICON_ID 0
#define IDC_QUIT 8

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

    // SetForegroundWindow doesn't say that it sets the last error
    // when it fails so we reset the last error here so we can look at
    // it's value anyway
    SetLastError(0);

    // We only show the popup if we're able to make ourselves the
    // "active" window (with input focus), otherwise, we will never get any window message
    // when the user clicks away from the popup window which means we
    // can't hide it, which feels very weird for the user.
    if (0 == SetForegroundWindow(hWnd)) {
        DWORD error = GetLastError();
        logf("SetForegroundWindow failed, error=%d", error);
        messageboxf("Error", "error: SetForegroundWindow failed, error=%u", error);
        return;
    }

    BOOL was_visible = ShowWindow(hWnd, SW_SHOW);
    logf("show window (was_visible=%d)", !!was_visible);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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

    HWND hWnd = CreateWindowExW(
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
    if (hWnd == NULL)
        fatal("CreateWindow failed, error=%d", GetLastError());

    if (!CreateWindowExW(
        0,
        L"BUTTON",
        L"Quit",
        WS_VISIBLE | WS_CHILD,
        25, 10,
        50, 30,
        hWnd,
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
        data.hWnd = hWnd;
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
        data.hWnd = hWnd;
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
        data.hWnd = hWnd;
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
