#include "window.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

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