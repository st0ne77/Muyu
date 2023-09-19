#include "window.h"
#include "resource.h"

#include <assert.h>
#include <memory>

namespace {
static const wchar_t *kWindowClass = L"SecurePlayerWindow";

static ULONG_PTR gdiplus_token = 1;
static Gdiplus::GdiplusStartupInput  m_gdiplusStartupInput;

static int kDefaultWidth = 105;
static int kDefaultHeight = 105;

static int kUpdateTimer = 1;

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;

static inline uint64_t get_clockfreq(void) {
	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}
	return clock_freq.QuadPart;
}

uint64_t GetOSTimeUsec(void) {
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);
	return current_time.QuadPart * 1000 * 1000 / get_clockfreq();
}

static BOOL CALLBACK EnumMonitorProc(HMONITOR handle, HDC hdc, LPRECT rect, LPARAM param) {

    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(handle, (LPMONITORINFO)&mi) && (mi.dwFlags & MONITORINFOF_PRIMARY) != 0) {
        DEVMODEW device_mode;
        device_mode.dmSize = sizeof(device_mode);
        device_mode.dmDriverExtra = 0;
        if (EnumDisplaySettingsExW(mi.szDevice, ENUM_CURRENT_SETTINGS, &device_mode, 0) && device_mode.dmPelsWidth > 0 && device_mode.dmPelsHeight > 0) {
            SIZE *size = reinterpret_cast<SIZE*>(param);
            size->cx = device_mode.dmPelsWidth;
            size->cy = device_mode.dmPelsHeight;
        }
        return false;
    }
    return true;
}

struct WAVE_HEADER {                  //44bytes
    char    riff_sig[4];
    long    waveform_chunk_size;
    char    wave_sig[4];
    char    format_sig[4];
    long    format_chunk_size;
    short   format_tag;
    short   channels;
    long    sample_rate;
    long    bytes_per_sec;
    short   block_align;
    short   bits_per_sample;
    char    data_sig[4];
    long    data_size;
};

}

Window::Window() {

    Gdiplus::GdiplusStartup(&gdiplus_token, &m_gdiplusStartupInput, NULL);

    HINSTANCE instance = GetModuleHandle(nullptr);
    HRSRC res_info = FindResource(instance, MAKEINTRESOURCE(ID_RC_FILE), MAKEINTRESOURCE(RC_IMAGE));
    DWORD size = SizeofResource(instance, res_info);
    if (res_info) {
        HGLOBAL hres = LoadResource(instance, res_info);
        if (hres) {
            LPVOID pres = LockResource(hres);
            if (pres) {
                HGLOBAL hGlobal = ::GlobalAlloc(GHND, size);
                if (hGlobal) {
                    void* pBuffer = ::GlobalLock(hGlobal);
                    if (pBuffer) {
                        memcpy(pBuffer, pres, size);
                        IStream* stream = nullptr;
                        HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &stream);
                        if (SUCCEEDED(hr)) {
                            // pStream now owns the global handle and will invoke GlobalFree on release
                            image_ = std::make_unique<Gdiplus::Bitmap>(stream);
                        }
                    }
                }

                UnlockResource(hres);
            }
            FreeResource(hres);
        }
    }

    res_info = FindResource(instance, MAKEINTRESOURCE(ID_RC_FILE), MAKEINTRESOURCE(RC_WAV));
    if (res_info) {
        hres_ = LoadResource(instance, res_info);
        if (hres_) {
            pres_ = LockResource(hres_);
        }
    }

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = HandleMsgSetup;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    HDC hdcScreen = GetDC(NULL);
    SIZE screen_size{GetDeviceCaps(hdcScreen, HORZRES), GetDeviceCaps(hdcScreen, VERTRES)};
    ReleaseDC(NULL, hdcScreen);

    hwnd_ = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                         kWindowClass,
                         L"Secure Player",
                         WS_POPUP,
                         screen_size.cx - kDefaultWidth,
                         screen_size.cy - kDefaultHeight - 100,
                         kDefaultWidth,
                         kDefaultHeight,
                         nullptr,
                         nullptr,
                         GetModuleHandle(nullptr),
                         this);

    if (!hwnd_) {
        assert(0);
    }

    SetTimer(hwnd_,
            kUpdateTimer,
            50,
            nullptr);

    exit_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    play_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    play_thread_ = CreateThread(nullptr, 0, &Window::PlayThread, this, 0, nullptr);
}

Window::~Window() {
    KillTimer(hwnd_, kUpdateTimer);

    SetEvent(exit_event_);
    WaitForSingleObject(play_thread_, INFINITE);
    CloseHandle(exit_event_);
    CloseHandle(play_thread_);

    DestroyWindow(hwnd_);
    image_.reset();

    UnlockResource(hres_);
    FreeResource(hres_);

    Gdiplus::GdiplusShutdown(gdiplus_token);
}

void Window::Show() {
    ShowWindow(hwnd_, SW_SHOWDEFAULT);
}

LRESULT CALLBACK Window::HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // use create parameter passed in from CreateWindow() to store window class pointer at WinAPI side
    if (msg == WM_NCCREATE) {
        // extract ptr to window class from creation data
        const CREATESTRUCTW *const pCreate = reinterpret_cast<CREATESTRUCTW *>(lParam);
        Window *const pWnd = static_cast<Window *>(pCreate->lpCreateParams);
        // set WinAPI-managed user data to store ptr to window instance
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
        // set message proc to normal (non-setup) handler now that setup is finished
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::HandleMsgThunk));
        // forward message to window instance handler
        return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
    }
    // if we get a message before the WM_NCCREATE message, handle with default handler
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Window::HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // retrieve ptr to window instance
    Window *const pWnd = reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    // forward message to window instance handler
    return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    // we don't want the DefProc to handle this message because
    // we want our destructor to destroy the window, so return 0 instead of break
    case WM_CLOSE: {
        PostQuitMessage(0);
        return 0;
    }
    case WM_CREATE: {
        Draw(hWnd);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        ++music_count_;
        ++click_count_;
        SetEvent(play_event_);
        last_press_time_ = GetOSTimeUsec();
        return 0;
    }
    case WM_PAINT: {
        return 0;
    }
    case WM_TIMER: {
        uint64_t current = GetOSTimeUsec();

        uint64_t diff = current - last_press_time_;
        if (diff > 3000000) {
            break;
        }
        Draw(hWnd);
        break;
    }
    case WM_NCHITTEST: {
        if (GetKeyState(VK_LCONTROL) < 0) {
            return HTCAPTION;
        } else {
            return HTCLIENT;
        }
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Window::Draw(HWND hwnd) {
    RECT rect;
    ::GetWindowRect(hwnd, &rect);
    SIZE wndSize = {rect.right - rect.left, rect.bottom - rect.top};

    HDC hdcScreen = GetDC(NULL);
    HDC hdc = ::CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = ::CreateCompatibleBitmap(hdcScreen, wndSize.cx, wndSize.cy);
    HGDIOBJ hBmpOld = ::SelectObject(hdc, hBmp);

    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

    Gdiplus::Rect windowRect{0, 0, kDefaultWidth, kDefaultHeight};

    uint64_t current = GetOSTimeUsec();
    uint64_t diff = current - last_press_time_;
    if (diff < 200000) {
        float diff = 4.0f;
        std::unique_ptr<Gdiplus::Bitmap> zoom(image_->Clone(diff, diff, image_->GetWidth() - diff * 2.0f, image_->GetHeight() - diff * 2.0f, image_->GetPixelFormat()));
        graphics.DrawImage(zoom.get(), windowRect);
    } else {
        graphics.DrawImage(image_.get(), windowRect);
    }

    if (diff < 2000000) {
        Gdiplus::REAL size = 12;
        Gdiplus::Font font(L"微软雅黑", size);

        uint8_t alpha = (uint8_t)((2000000 - (current - last_press_time_)) * 255 / 2000000);

        Gdiplus::SolidBrush textBrush(Gdiplus::Color(alpha, 255, 0, 0));

        wchar_t buffer[1024];
        swprintf(buffer, L"功德+%lld", click_count_);

        Gdiplus::StringFormat sf;
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsMeasureTrailingSpaces );
        sf.SetLineAlignment(Gdiplus::StringAlignmentNear); //StringAlignmentNear StringAlignmentCenter
        sf.SetAlignment(Gdiplus::StringAlignmentFar);

        graphics.DrawString(buffer, lstrlen(buffer), &font, Gdiplus::RectF(0.0f, 0.0f, (float)wndSize.cx, (float)wndSize.cy), &sf, &textBrush);
    }

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT ptSrc{0, 0};
    POINT ptDest{rect.left, rect.top};
    UpdateLayeredWindow(hwnd, hdcScreen, &ptDest, &wndSize, hdc, &ptSrc, 0, &blend, ULW_ALPHA);

    ::SelectObject(hdc, hBmpOld);
    ::DeleteObject(hBmp);
    ::DeleteDC(hdc);
    ::ReleaseDC(nullptr, hdcScreen);
}

DWORD Window::PlayThread(LPVOID arg) {
    Window *that = reinterpret_cast<Window*>(arg);

    HANDLE sigs[2] = {that->exit_event_, that->play_event_};
    while (WaitForMultipleObjects(2, sigs, FALSE, INFINITE) != WAIT_OBJECT_0) {
        if (that->music_count_.load() > 0) {
            sndPlaySound((LPCTSTR)that->pres_, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
            --that->music_count_;
        }
    }

    return 0;
}