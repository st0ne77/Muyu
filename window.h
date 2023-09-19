#pragma once
#include "CWaveOut.h"

#include <memory>
#include <Windows.h>
#include <gdiplus.h>
#include <atomic>
#include <set>

class Window {
public:
    Window();
    ~Window();

    void Show();
private:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    static LRESULT CALLBACK HandleMsgSetup( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam );
    static LRESULT CALLBACK HandleMsgThunk( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam );
    LRESULT HandleMsg( HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam );

    void Draw(HWND hwnd);

    static DWORD WINAPI PlayThread(LPVOID arg);

    HWND hwnd_;

    std::unique_ptr<Gdiplus::Bitmap> image_;

    HGLOBAL hres_ = nullptr;
    void *pres_ = nullptr;

    std::vector<char> pcm_;

    uint64_t last_press_time_ = 0;

    uint64_t click_count_ = 0;

    HANDLE exit_event_ = nullptr;
    HANDLE play_event_ = nullptr;
    HANDLE play_thread_ = nullptr;

    std::atomic_uint32_t music_count_ = 0;
};