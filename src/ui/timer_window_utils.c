#include "timer_window_utils.h"
#include "timer_core.h"
#include <dwmapi.h>

// ===========================================
// 窗口样式设置
// ===========================================

void SetWindowRoundedCorners(HWND hwnd, int cornerRadius) {
    if (!hwnd) return;

    BOOL dwmEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&dwmEnabled)) && dwmEnabled) {
        if (cornerRadius <= 0) {
            DWORD cornerPref = 1; // DWMWCP_DONOTROUND
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
            BOOL ncrenderingEnabled = FALSE;
            DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncrenderingEnabled, sizeof(ncrenderingEnabled));
            MARGINS margins = {0, 0, 0, 0};
            DwmExtendFrameIntoClientArea(hwnd, &margins);
            SetWindowRgn(hwnd, NULL, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
            return;
        }

        DWORD cornerPref = 2; // DWMWCP_ROUND
        HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
        if (SUCCEEDED(hr)) {
            SetWindowRgn(hwnd, NULL, TRUE);
            return;
        }
    }

    if (cornerRadius <= 0) {
        SetWindowRgn(hwnd, NULL, TRUE);
        return;
    }

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, cornerRadius * 2, cornerRadius * 2);
    if (hRgn) {
        SetWindowRgn(hwnd, hRgn, TRUE);
    }
}

void SetWindowShadow(HWND hwnd, BOOL enable) {
    if (!hwnd) return;

    BOOL dwmEnabled = FALSE;
    HRESULT hr = DwmIsCompositionEnabled(&dwmEnabled);
    if (FAILED(hr) || !dwmEnabled) return;

    if (enable) {
        enum DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        BOOL ncRenderingEnabled = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncRenderingEnabled, sizeof(ncRenderingEnabled));
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        BOOL enableShadow = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &enableShadow, sizeof(enableShadow));
    } else {
        MARGINS margins = {-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        BOOL ncRenderingEnabled = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncRenderingEnabled, sizeof(ncRenderingEnabled));
        enum DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
        BOOL disableShadow = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &disableShadow, sizeof(disableShadow));
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_BORDER | WS_DLGFRAME | WS_THICKFRAME);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_DRAWFRAME);
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
    }
}
