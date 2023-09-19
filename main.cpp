#include "window.h"

bool CheckInstance() {
    HANDLE hSingleInstanceMutex = CreateMutex(NULL, FALSE, L"st0ne77_MUYU");
    if (hSingleInstanceMutex != NULL) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            return true;
        }
    }

    return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    if (CheckInstance()) {
        return -1;
    }

    Window window;
    window.Show();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // TranslateMessage will post auxilliary WM_CHAR messages from key msgs
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}