#include <iostream>
#include <Windows.h>
#include <gdiplus.h>#pragma comment(lib,"gdiplus.lib")
typedef BOOL (*UPDATELAYEREDWINDOWFUNCTION)(HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF, BLENDFUNCTION *, DWORD);
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_LBUTTONDOWN:
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
int main()
{ //// 变量定义//ULONG_PTR gdiplusStartupToken;Gdiplus::GdiplusStartupInput gdiInput;HWND hwnd;MSG messages;WNDCLASSEX wincl;
    HINSTANCE hThisInstance = GetModuleHandle(0);
    char szClassName[] = "PNGDialog";
    RECT wndRect;
    SIZE wndSize;
    HDC hdc;
    HDC memDC;
    HBITMAP memBitmap;
    HMODULE hUser32;
    UPDATELAYEREDWINDOWFUNCTION UpdateLayeredWindow;
    HDC screenDC;
    POINT ptSrc;
    BLENDFUNCTION blendFunction;
    //// 初始化 GDI+//
    Gdiplus::GdiplusStartup(&gdiplusStartupToken, &gdiInput, NULL);
    //// 定义窗口结构//
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProcedure;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = NULL;
    wincl.hIconSm = NULL;
    wincl.hCursor = NULL;
    wincl.lpszMenuName = NULL;
    w
        incl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = (HBRUSH)COLOR_BACKGROUND; //// 注册窗口类//
    if (!RegisterClassEx(&wincl))
    {
        printf("RegisterClassEx error : %d \n", GetLastError());
        return 0;
    } //// 创建窗口//
    hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST, szClassName, "PNGDialog Example Application",
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 500, HWND_DESKTOP, NULL,
                          hThisInstance, NULL); //// 使窗口在屏幕上可见//
    ShowWindow(hwnd, SW_SHOW);
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    if (style & WS_CAPTION)
        style ^= WS_CAPTION;
    if (style & WS_THICKFRAME)
        style ^= WS_THICKFRAME;
    if (style & WS_SYSMENU)
        style ^= WS_SYSMENU;
    ::SetWindowLong(hwnd, GWL_STYLE, style);
    style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
    if (style & WS_EX_APPWINDOW)
        style ^= WS_EX_APPWINDOW;
    ::SetWindowLong(hwnd, GWL_EXSTYLE, style);
    //// 使用 GDI+ 加载图片//
    ::GetWindowRect(hwnd, &wndRect);
    wndSize = {wndRect.right - wndRect.left, wndRect.bottom - wndRect.top};
    hdc = ::GetDC(hwnd);
    memDC = ::CreateCompatibleDC(hdc);
    memBitmap = ::CreateCompatibleBitmap(hdc, wndSize.cx, wndSize.cy);
    ::SelectObject(memDC, memBitmap);
    Gdiplus::Image image(L"pic.png");
    Gdiplus::Graphics graphics(memDC);
    graphics.DrawImage(&image, 0, 0, wndSize.cx, wndSize.cy); //// 获取 UpdateLayeredWindow 函数地址//
    hUser32 = ::LoadLibrary("User32.dll");
    if (!hUser32)
    {
        return FALSE;
    }
    UpdateLayeredWindow = (UPDATELAYEREDWINDOWFUNCTION)::GetProcAddress(hUser32, "UpdateLayeredWindow");
    if (!UpdateLayeredWindow)
    {
        return FALSE;
    } //// 获取屏幕 DC//
    screenDC = GetDC(NULL);
    ptSrc = {0, 0}; //// 绘制窗口//
    blendFunction.AlphaFormat = AC_SRC_ALPHA;
    blendFunction.BlendFlags = 0;
    blendFunction.BlendOp = AC_SRC_OVER;
    blendFunction.SourceConstantAlpha = 255;
    UpdateLayeredWindow(hwnd, screenDC, &ptSrc, &wndSize, memDC, &ptSrc, 0, &blendFunction, 2);
    ::DeleteDC(memDC);
    ::DeleteObject(memBitmap);
    //// 反截屏(会失败，原因是只能对 WS_EX_TOPMOST 类型的窗口使用)///*if (!SetWindowDisplayAffinity(hwnd, WDA_MONITOR)){printf("SetWindowDisplayAffinity error : %d \n", GetLastError());}*///// 进入消息循环//
    while (GetMessage(&messages, NULL, 0, 0))
    {
        TranslateMessage(&messages);
        DispatchMessage(&messages);
    }
    Gdiplus::GdiplusShutdown(gdiplusStartupToken);
    (void)getchar();
    return 0;
}