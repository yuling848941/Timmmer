#include "timer_buffer.h"
#include "timer_core.h"
#include "timer_ui.h"
#include "timer_fonts.h"

// ===========================================
// 双缓冲位图管理
// ===========================================

void InitBackBuffer(void) {
    if (g_timerState.hdcBackBuffer != NULL) {
        return; // 已初始化
    }

    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        g_timerState.hdcBackBuffer = CreateCompatibleDC(hdcScreen);
        g_timerState.hbmBackBuffer = CreateCompatibleBitmap(hdcScreen,
                                                            g_timerState.windowWidth,
                                                            g_timerState.windowHeight);
        if (g_timerState.hbmBackBuffer) {
            SelectObject(g_timerState.hdcBackBuffer, g_timerState.hbmBackBuffer);
        }
        g_timerState.backBufferWidth = g_timerState.windowWidth;
        g_timerState.backBufferHeight = g_timerState.windowHeight;
        g_timerState.backBufferDirty = TRUE;
        ReleaseDC(NULL, hdcScreen);
    }
}

void CleanupBackBuffer(void) {
    if (g_timerState.hbmBackBuffer) {
        DeleteObject(g_timerState.hbmBackBuffer);
        g_timerState.hbmBackBuffer = NULL;
    }
    if (g_timerState.hdcBackBuffer) {
        DeleteDC(g_timerState.hdcBackBuffer);
        g_timerState.hdcBackBuffer = NULL;
    }
    g_timerState.backBufferWidth = 0;
    g_timerState.backBufferHeight = 0;
    g_timerState.backBufferDirty = FALSE;
}

void EnsureBackBufferSize(int width, int height) {
    if (g_timerState.hbmBackBuffer &&
        width == g_timerState.backBufferWidth &&
        height == g_timerState.backBufferHeight) {
        return; // 尺寸匹配，无需重建
    }

    // 清理旧位图
    CleanupBackBuffer();

    // 创建新位图
    HDC hdcScreen = GetDC(NULL);
    if (hdcScreen) {
        g_timerState.hdcBackBuffer = CreateCompatibleDC(hdcScreen);
        g_timerState.hbmBackBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
        if (g_timerState.hbmBackBuffer) {
            SelectObject(g_timerState.hdcBackBuffer, g_timerState.hbmBackBuffer);
        }
        g_timerState.backBufferWidth = width;
        g_timerState.backBufferHeight = height;
        g_timerState.backBufferDirty = TRUE;
        ReleaseDC(NULL, hdcScreen);
    }
}

// ===========================================
// 数字预渲染缓存
// ===========================================

void CleanupDigitCache(void) {
    for (int i = 0; i < 10; i++) {
        if (g_timerState.hbmDigitCache[i]) {
            DeleteObject(g_timerState.hbmDigitCache[i]);
            g_timerState.hbmDigitCache[i] = NULL;
        }
    }
    if (g_timerState.hbmColonCache) {
        DeleteObject(g_timerState.hbmColonCache);
        g_timerState.hbmColonCache = NULL;
    }
    if (g_timerState.hbmDotCache) {
        DeleteObject(g_timerState.hbmDotCache);
        g_timerState.hbmDotCache = NULL;
    }
    g_timerState.digitCacheFontSize = 0;
    g_timerState.digitCacheValid = FALSE;
}

void BuildDigitCache(int fontSize) {
    if (g_timerState.digitCacheValid && fontSize == g_timerState.digitCacheFontSize) {
        return;
    }

    CleanupDigitCache();

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    HFONT hFont = GetMonospaceFont(fontSize);
    if (!hFont) hFont = GetDefaultTimerFont(fontSize);
    if (!hFont) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return;
    }

    HFONT oldFont = (HFONT)SelectObject(hdcMem, hFont);
    SetTextAlign(hdcMem, TA_TOP | TA_LEFT | TA_BASELINE);
    SetBkMode(hdcMem, TRANSPARENT);

    const char* digits = "0123456789:.";
    for (int i = 0; digits[i] != '\0'; i++) {
        char digitStr[2] = {digits[i], '\0'};
        SIZE textSize;
        GetTextExtentPointA(hdcMem, digitStr, 1, &textSize);

        int bmpWidth = textSize.cx + 4;
        int bmpHeight = textSize.cy + 4;

        HBITMAP hbmDigit = CreateCompatibleBitmap(hdcScreen, bmpWidth, bmpHeight);
        if (hbmDigit) {
            HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hbmDigit);
            RECT rect = {0, 0, bmpWidth, bmpHeight};
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdcMem, &rect, hBrush);
            DeleteObject(hBrush);
            SetTextColor(hdcMem, RGB(255, 255, 255));
            TextOutA(hdcMem, 2, 2, digitStr, 1);
            SelectObject(hdcMem, oldBmp);

            if (digits[i] >= '0' && digits[i] <= '9') g_timerState.hbmDigitCache[digits[i] - '0'] = hbmDigit;
            else if (digits[i] == ':') g_timerState.hbmColonCache = hbmDigit;
            else if (digits[i] == '.') g_timerState.hbmDotCache = hbmDigit;
        }
    }

    SelectObject(hdcMem, oldFont);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    g_timerState.digitCacheFontSize = fontSize;
    g_timerState.digitCacheValid = TRUE;
}

// ===========================================
// 分层窗口渲染
// ===========================================

// 保存 DIB 位图的像素指针
static void* g_layeredBufferBits = NULL;

void CleanupLayeredBuffer(void) {
    if (g_timerState.hbmLayeredBuffer) {
        DeleteObject(g_timerState.hbmLayeredBuffer);
        g_timerState.hbmLayeredBuffer = NULL;
    }
    if (g_timerState.hdcLayeredBuffer) {
        DeleteDC(g_timerState.hdcLayeredBuffer);
        g_timerState.hdcLayeredBuffer = NULL;
    }
    g_timerState.layeredBufferSize.cx = 0;
    g_timerState.layeredBufferSize.cy = 0;
    g_layeredBufferBits = NULL;
}

void UpdateLayeredWindow_Render(void) {
    if (!g_timerState.hMainWnd) {
        return;
    }

    HWND hwnd = g_timerState.hMainWnd;
    int width = g_timerState.windowWidth;
    int height = g_timerState.windowHeight;

    // 确保缓冲区尺寸正确
    if (width != g_timerState.layeredBufferSize.cx ||
        height != g_timerState.layeredBufferSize.cy) {
        CleanupLayeredBuffer();
    }

    // 创建缓冲区
    if (!g_timerState.hdcLayeredBuffer) {
        HDC hdcScreen = GetDC(NULL);
        if (hdcScreen) {
            g_timerState.hdcLayeredBuffer = CreateCompatibleDC(hdcScreen);

            BITMAPINFOHEADER bi = {0};
            bi.biSize = sizeof(BITMAPINFOHEADER);
            bi.biWidth = width;
            bi.biHeight = -height;
            bi.biPlanes = 1;
            bi.biBitCount = 32;
            bi.biCompression = BI_RGB;

            g_timerState.hbmLayeredBuffer = CreateDIBSection(hdcScreen,
                (BITMAPINFO*)&bi, DIB_RGB_COLORS, &g_layeredBufferBits, NULL, 0);

            if (g_timerState.hbmLayeredBuffer) {
                SelectObject(g_timerState.hdcLayeredBuffer, g_timerState.hbmLayeredBuffer);
                if (g_layeredBufferBits) {
                    memset(g_layeredBufferBits, 0, width * height * 4);
                }
            }
            g_timerState.layeredBufferSize.cx = width;
            g_timerState.layeredBufferSize.cy = height;
            ReleaseDC(NULL, hdcScreen);
        }
    }

    if (!g_timerState.hdcLayeredBuffer || !g_timerState.hbmLayeredBuffer) {
        return;
    }

    HDC hdcMem = g_timerState.hdcLayeredBuffer;

    if (g_layeredBufferBits) {
        memset(g_layeredBufferBits, 0, width * height * 4);
    }

    SetTextColor(hdcMem, g_timerState.fontColor);
    SetBkMode(hdcMem, TRANSPARENT);

    UpdateDisplay(hdcMem);

    GdiFlush();

    if (g_layeredBufferBits) {
        DWORD* pixels = (DWORD*)g_layeredBufferBits;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;
                DWORD pixel = pixels[idx];
                BYTE b = (pixel >> 0) & 0xFF;
                BYTE g = (pixel >> 8) & 0xFF;
                BYTE r = (pixel >> 16) & 0xFF;
                if (r != 0 || g != 0 || b != 0) {
                    pixels[idx] = (pixel & 0x00FFFFFF) | 0xFF000000;
                }
            }
        }
    }

    POINT ptSrc = {0, 0};
    RECT rcWnd;
    GetWindowRect(hwnd, &rcWnd);
    POINT ptDst = {rcWnd.left, rcWnd.top};

    SIZE sizeNew = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    blend.SourceConstantAlpha = g_timerState.windowOpacity;

    UpdateLayeredWindow(hwnd, NULL, &ptDst, &sizeNew, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
}
