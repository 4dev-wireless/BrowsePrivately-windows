#include <windows.h>
#include <stdio.h>
#include <string.h>

// ---- Configuration ----------------------------------------
#define SSH_HOST     "129.158.43.105"
#define SSH_USER     "tunneluser"
#define KEY_FILE     "privatekey.ppk"
#define SOCKS_PORT   "1080"
#define PLINK_EXE    "plink.exe"
#define FIREFOX_EXE  "browser\\FirefoxPortable.exe"
#define HOST_KEY     "SHA256:1BrFcatLES3TfjIozObepbMPSACZpoTU94/T4OueAoA"
// -----------------------------------------------------------

HWND g_hSplash = NULL;

void ShowError(const char *msg) {
    if (g_hSplash) { DestroyWindow(g_hSplash); g_hSplash = NULL; }
    MessageBox(NULL, msg, "Browse Privately - Error", MB_ICONERROR | MB_OK);
}

int FileExists(const char *path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES);
}

HANDLE StartHidden(const char *cmd) {
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    char cmdCopy[2048];
    strncpy(cmdCopy, cmd, sizeof(cmdCopy) - 1);
    if (!CreateProcessA(NULL, cmdCopy, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return NULL;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

DWORD RunVisible(const char *cmd) {
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    char cmdCopy[2048];
    strncpy(cmdCopy, cmd, sizeof(cmdCopy) - 1);
    if (!CreateProcessA(NULL, cmdCopy, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return 0;
    }
    CloseHandle(pi.hThread);
    DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    return pid;
}

LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hBg = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);
        HICON hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1),
            IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR);
        if (hIcon) {
            DrawIconEx(hdc, rc.right/2 - 32, rc.bottom/2 - 50, hIcon, 64, 64, 0, NULL, DI_NORMAL);
            DestroyIcon(hIcon);
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(80, 80, 80));
        HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        HFONT hOld = SelectObject(hdc, hFont);
        RECT subRect = {0, rc.bottom/2 + 24, rc.right, rc.bottom/2 + 48};
        DrawTextA(hdc, "Establishing secure connection...", -1, &subRect, DT_CENTER | DT_SINGLELINE);
        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void ShowSplash(HINSTANCE hInst) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "BPSplash";
    wc.hCursor = LoadCursor(NULL, IDC_WAIT);
    RegisterClassA(&wc);
    int w = 320, h = 220;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    g_hSplash = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "BPSplash", "Browse Privately",
        WS_POPUP, x, y, w, h, NULL, NULL, hInst, NULL);
    ShowWindow(g_hSplash, SW_SHOW);
    UpdateWindow(g_hSplash);
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Try tunnel on a specific port, returns live plink handle or NULL
HANDLE TryTunnel(const char *port_arg, int wait_ms) {
    char cmd[2048];

    // Accept host key silently
    snprintf(cmd, sizeof(cmd),
        "%s -ssh -batch %s -i %s -hostkey %s %s@%s exit",
        PLINK_EXE, port_arg, KEY_FILE, HOST_KEY, SSH_USER, SSH_HOST);
    HANDLE hKey = StartHidden(cmd);
    if (hKey) {
        WaitForSingleObject(hKey, 10000);
        CloseHandle(hKey);
    }

    // Start SOCKS tunnel
    snprintf(cmd, sizeof(cmd),
        "%s -ssh -batch %s -i %s -hostkey %s -D %s -N %s@%s",
        PLINK_EXE, port_arg, KEY_FILE, HOST_KEY, SOCKS_PORT, SSH_USER, SSH_HOST);
    HANDLE hPlink = StartHidden(cmd);
    if (!hPlink) return NULL;

    // Wait then check if plink is still alive
    Sleep(wait_ms);
    DWORD exitCode = 0;
    GetExitCodeProcess(hPlink, &exitCode);
    if (exitCode == STILL_ACTIVE) {
        return hPlink;
    }

    CloseHandle(hPlink);
    return NULL;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    char cmd[2048];

    if (!FileExists(PLINK_EXE)) {
        ShowError("Cannot find plink.exe\nMake sure it is in the same folder as this launcher.");
        return 1;
    }
    if (!FileExists(KEY_FILE)) {
        ShowError("Cannot find privatekey.ppk\nMake sure it is in the same folder as this launcher.");
        return 1;
    }
    if (!FileExists(FIREFOX_EXE)) {
        ShowError("Cannot find browser\\FirefoxPortable.exe\nMake sure the browser folder is beside this launcher.");
        return 1;
    }

    ShowSplash(hInstance);

    // Try port 22 first (4 second wait)
    HANDLE hPlink = TryTunnel("-P 22", 4000);

    // Fall back to port 465 (10 second wait for restricted networks)
    if (!hPlink) {
        hPlink = TryTunnel("-P 465", 10000);
    }

    // Both failed
    if (!hPlink) {
        ShowError("Could not establish a secure tunnel.\nThe server may be down or unreachable.\nPlease try again later.");
        return 1;
    }

    // Close splash and open Firefox
    if (g_hSplash) { DestroyWindow(g_hSplash); g_hSplash = NULL; }

    snprintf(cmd, sizeof(cmd), "%s -no-remote", FIREFOX_EXE);
    DWORD firefoxPID = RunVisible(cmd);

    if (!firefoxPID) {
        ShowError("Failed to launch Firefox.");
        TerminateProcess(hPlink, 0);
        CloseHandle(hPlink);
        return 1;
    }

    // Wait for Firefox to close
    HANDLE hFirefox = OpenProcess(SYNCHRONIZE, FALSE, firefoxPID);
    if (hFirefox) {
        WaitForSingleObject(hFirefox, INFINITE);
        CloseHandle(hFirefox);
    }

    // Kill plink
    TerminateProcess(hPlink, 0);
    CloseHandle(hPlink);

    return 0;
}
