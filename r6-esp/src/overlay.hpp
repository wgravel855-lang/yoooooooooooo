#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include "math.hpp"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")

// Transparent, click-through, always-on-top layered window with a Direct2D
// render target. It covers the full virtual desktop and draws the ESP each
// frame, compositing over the game via DWM. Because this is a separate
// top-level window and never writes into the game's address space, it stays
// outside anything the game's own process can inspect.
class Overlay {
public:
    int width  = 0;
    int height = 0;

    bool create() {
        width  = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"r6_overlay";
        RegisterClassExW(&wc);

        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, width, height,
            nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd) return false;

        // Extend the DWM glass frame over the full client area. With a
        // premultiplied-alpha D2D target cleared to alpha 0, DWM composites the
        // drawn pixels and leaves the rest fully transparent. Do NOT also call
        // SetLayeredWindowAttributes here — a color key disables the per-pixel
        // alpha this relies on.
        MARGINS m = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &m);

        ShowWindow(hwnd, SW_SHOW);
        return initD2D();
    }

    void pumpMessages() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void begin() {
        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0, 0.f)); // fully transparent
    }

    void end() {
        rt->EndDraw();
    }

    void box(float x, float y, float w, float h, D2D1::ColorF c, float stroke = 1.5f) {
        brush->SetColor(c);
        rt->DrawRectangle(D2D1::RectF(x, y, x + w, y + h), brush, stroke);
    }

    // Filled rect, used for health-bar backgrounds/fills.
    void fill(float x, float y, float w, float h, D2D1::ColorF c) {
        brush->SetColor(c);
        rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush);
    }

    void line(Vec2 a, Vec2 b, D2D1::ColorF c, float stroke = 1.5f) {
        brush->SetColor(c);
        rt->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(b.x, b.y), brush, stroke);
    }

    void text(const std::wstring& s, float x, float y, D2D1::ColorF c) {
        brush->SetColor(c);
        rt->DrawTextW(s.c_str(), (UINT32)s.size(), textFmt,
                      D2D1::RectF(x, y, x + 400.f, y + 20.f), brush);
    }

    HWND handle() const { return hwnd; }

private:
    HWND hwnd = nullptr;
    ID2D1Factory*          factory = nullptr;
    ID2D1HwndRenderTarget* rt      = nullptr;
    ID2D1SolidColorBrush*  brush   = nullptr;
    IDWriteFactory*        dw      = nullptr;
    IDWriteTextFormat*     textFmt = nullptr;

    bool initD2D() {
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory)))
            return false;

        if (FAILED(factory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(
                    D2D1_RENDER_TARGET_TYPE_DEFAULT,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                                      D2D1_ALPHA_MODE_PREMULTIPLIED)),
                D2D1::HwndRenderTargetProperties(
                    hwnd, D2D1::SizeU(width, height)),
                &rt)))
            return false;

        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush);

        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                            __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(&dw));
        dw->CreateTextFormat(L"Consolas", nullptr,
                             DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                             DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &textFmt);
        textFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        return true;
    }
};
