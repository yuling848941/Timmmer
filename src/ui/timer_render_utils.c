#include "timer_render_utils.h"
#include <stdlib.h>
#include <wchar.h>

void BlendPixel(BYTE* pixels, int idx, BYTE r, BYTE g, BYTE b, float alpha) {
    if (alpha <= 0.01f) return;
    if (alpha >= 0.99f) {
        pixels[idx] = b; pixels[idx+1] = g; pixels[idx+2] = r; pixels[idx+3] = 255;
        return;
    }
    BYTE a = (BYTE)(alpha * 255);
    pixels[idx] = (pixels[idx] * (255 - a) + b * a) >> 8;
    pixels[idx+1] = (pixels[idx+1] * (255 - a) + g * a) >> 8;
    pixels[idx+2] = (pixels[idx+2] * (255 - a) + r * a) >> 8;
    pixels[idx+3] = a > pixels[idx+3] ? a : pixels[idx+3];
}

float RoundedRectSDF(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float dx = fabsf(px - cx) - hw + r;
    float dy = fabsf(py - cy) - hh + r;
    float len = sqrtf(fmaxf(dx, 0.0f) * fmaxf(dx, 0.0f) + fmaxf(dy, 0.0f) * fmaxf(dy, 0.0f));
    return len + fminf(fmaxf(dx, dy), 0.0f) - r;
}

void DrawSoftShadowSDF(BYTE* pixels, int bufW, int bufH, int mx, int my, int mw, int mh, int r, int shadowSize, int offsetY, float maxOpacity) {
    float cx = mx + mw / 2.0f;
    float cy = my + mh / 2.0f + offsetY;
    float hw = mw / 2.0f;
    float hh = mh / 2.0f;

    for (int y = 0; y < bufH; y++) {
        for (int x = 0; x < bufW; x++) {
            float d = RoundedRectSDF((float)x, (float)y, cx, cy, hw, hh, (float)r);
            if (d < shadowSize) {
                float alpha = 0.0f;
                if (d <= 0.0f) {
                    alpha = maxOpacity;
                } else {
                    float t = 1.0f - (d / shadowSize);
                    alpha = maxOpacity * t * t * (3.0f - 2.0f * t);
                }
                BlendPixel(pixels, (y * bufW + x) * 4, 0, 0, 0, alpha);
            }
        }
    }
}

// 圆角矩形在像素中心的覆盖率：直边落在像素边界上，整数坐标的矩形正好得到整像素，
// 圆角处按 SDF 距离做 1px 过渡（旧实现在整数坐标处采样，圆角会偏半个像素且边缘发毛）
static float RoundedRectCoverage(int px, int py, float cx, float cy, float hw, float hh, float r) {
    float d = RoundedRectSDF(px + 0.5f, py + 0.5f, cx, cy, hw, hh, r);
    float cov = 0.5f - d;
    if (cov <= 0.0f) return 0.0f;
    return cov < 1.0f ? cov : 1.0f;
}

void FillRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a) {
    float cx = x + w / 2.0f, cy = y + h / 2.0f, hw = w / 2.0f, hh = h / 2.0f;
    for (int py = y - 1; py < y + h + 1; py++) {
        if (py < 0 || py >= bufH) continue;
        for (int px = x - 1; px < x + w + 1; px++) {
            if (px < 0 || px >= bufW) continue;
            float cov = RoundedRectCoverage(px, py, cx, cy, hw, hh, (float)rx);
            if (cov <= 0.0f) continue;
            BlendPixel(pixels, (py * bufW + px) * 4, r, g, b, cov * (a / 255.0f));
        }
    }
}

void FillStripedRoundedRectAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, BYTE r, BYTE g, BYTE b, BYTE a) {
    float cx = x + w / 2.0f, cy = y + h / 2.0f, hw = w / 2.0f, hh = h / 2.0f;
    for (int py = y - 1; py < y + h + 1; py++) {
        if (py < 0 || py >= bufH) continue;
        for (int px = x - 1; px < x + w + 1; px++) {
            if (px < 0 || px >= bufW) continue;
            float cov = RoundedRectCoverage(px, py, cx, cy, hw, hh, (float)rx);
            if (cov <= 0.0f) continue;

            BYTE pr = r, pg = g, pb = b;
            int stripeWidth = 6;
            if (((px - x + py - y) / stripeWidth) % 2 == 0) {
                pr = (BYTE)fmaxf(0, pr - 30);
                pg = (BYTE)fmaxf(0, pg - 30);
                pb = (BYTE)fmaxf(0, pb - 30);
            }

            BlendPixel(pixels, (py * bufW + px) * 4, pr, pg, pb, cov * (a / 255.0f));
        }
    }
}

void DrawRoundedRectOutlineAA(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h, int thickness, BYTE r, BYTE g, BYTE b, BYTE a) {
    if (thickness <= 0) return;
    float t = (float)thickness;
    // 描边中心线相对矩形边缘内缩 thickness/2：thickness=1 时中心线正好在矩形最外一圈像素的中心上，
    // 得到真正 1px 的实线（旧实现在整数坐标处采样，1px 描边实际铺满两列像素）
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;
    float hw = w / 2.0f - t / 2.0f;
    float hh = h / 2.0f - t / 2.0f;
    float rad = (float)rx - t / 2.0f;
    if (rad < 0.0f) rad = 0.0f;

    for (int py = y - 2; py <= y + h + 2; py++) {
        if (py < 0 || py >= bufH) continue;
        for (int px = x - 2; px <= x + w + 2; px++) {
            if (px < 0 || px >= bufW) continue;
            float d = fabsf(RoundedRectSDF(px + 0.5f, py + 0.5f, cx, cy, hw, hh, rad));
            float cov = t / 2.0f + 0.5f - d;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            BlendPixel(pixels, (py * bufW + px) * 4, r, g, b, cov * (a / 255.0f));
        }
    }
}

void DrawHairlineH(BYTE* pixels, int bufW, int x0, int x1, int y, float alpha, int thickness) {
    if (thickness <= 0) return;
    for (int k = 0; k < thickness; k++) {
        int py = y + k;
        if (py < 0) continue;
        for (int px = x0; px < x1; px++) {
            BlendPixel(pixels, (py * bufW + px) * 4, 0, 0, 0, alpha);
        }
    }
}

void DrawSwitchWin11Scaled(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn, UINT dpi) {
    int sw = ScalePx(34, dpi), sh = ScalePx(18, dpi), ts = ScalePx(10, dpi);
    int cy = y + (rowHeight - sh) / 2;
    int r = sh / 2;
    if (isOn) {
        FillRoundedRectAA(pixels, bufW, bufH, r, r, x, cy, sw, sh, 0, 95, 184, 255);
        FillRoundedRectAA(pixels, bufW, bufH, ts / 2, ts / 2, x + sw - ts - ScalePx(4, dpi), cy + ScalePx(4, dpi), ts, ts, 255, 255, 255, 255);
    } else {
        FillRoundedRectAA(pixels, bufW, bufH, r, r, x, cy, sw, sh, 255, 255, 255, 255);
        DrawRoundedRectOutlineAA(pixels, bufW, bufH, r, r, x, cy, sw, sh, max(1, ScalePx(1, dpi)), 160, 160, 160, 255);
        FillRoundedRectAA(pixels, bufW, bufH, ts / 2, ts / 2, x + ScalePx(4, dpi), cy + ScalePx(4, dpi), ts, ts, 110, 110, 110, 255);
    }
}

void DrawSwitchWin11(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn) {
    DrawSwitchWin11Scaled(pixels, bufW, bufH, x, y, rowHeight, isOn, 96);
}

// ===========================================
// 字体族选择
// ===========================================

typedef struct {
    const wchar_t* face;
    int count;
} FamilyQuery;

static int CALLBACK FamilyEnumProc(const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD type, LPARAM lp) {
    (void)tm; (void)type;
    FamilyQuery* q = (FamilyQuery*)lp;
    if (_wcsicmp(lf->lfFaceName, q->face) == 0) q->count++;
    return 1;
}

static BOOL FontFamilyExists(const wchar_t* face) {
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    FamilyQuery q = { face, 0 };
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, FamilyEnumProc, (LPARAM)&q, 0);
    ReleaseDC(NULL, dc);
    return q.count > 0;
}

const wchar_t* GetUiFontFamily(void) {
    static int cached = -1;   // -1 未探测，0 = Segoe UI，1 = Segoe UI Variable Text
    if (cached < 0) cached = FontFamilyExists(L"Segoe UI Variable Text") ? 1 : 0;
    return cached ? L"Segoe UI Variable Text" : L"Segoe UI";
}

const wchar_t* GetIconFontFamily(void) {
    static int cached = -1;
    if (cached < 0) cached = FontFamilyExists(L"Segoe Fluent Icons") ? 1 : 0;
    return cached ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
}

HFONT CreateUiFont(int emPx, int weight, BOOL semiboldDisplay) {
    const wchar_t* face = GetUiFontFamily();
    if (semiboldDisplay && FontFamilyExists(L"Segoe UI Variable Display Semibold")) {
        face = L"Segoe UI Variable Display Semibold";
    }
    return CreateFontW(-emPx, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, 0,
                       CLEARTYPE_QUALITY, 0, face);
}

// ===========================================
// DPI
// ===========================================

typedef HANDLE (WINAPI *SetThreadDpiAwarenessContextFn)(HANDLE);
typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);

static SetThreadDpiAwarenessContextFn GetSetThreadDpiAwarenessContext(void) {
    return (SetThreadDpiAwarenessContextFn)(void(*)(void))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
}

HANDLE EnterPerMonitorDpiContext(void) {
    SetThreadDpiAwarenessContextFn fn = GetSetThreadDpiAwarenessContext();
    if (!fn) return NULL;
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
    return fn((HANDLE)(INT_PTR)-4);
}

void LeaveDpiContext(HANDLE prev) {
    SetThreadDpiAwarenessContextFn fn = GetSetThreadDpiAwarenessContext();
    if (fn && prev) fn(prev);
}

UINT GetWindowDpi(HWND hwnd) {
    GetDpiForWindowFn fn = (GetDpiForWindowFn)(void(*)(void))
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn) {
        UINT dpi = fn(hwnd);
        if (dpi) return dpi;
    }
    HDC dc = GetDC(NULL);
    UINT dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi ? dpi : 96;
}

int ScalePx(int px96, UINT dpi) {
    return MulDiv(px96, (int)dpi, 96);
}

UINT QueryMonitorDpi(HMONITOR monitor) {
    typedef HRESULT (WINAPI *GetDpiForMonitorFn)(HMONITOR, int, UINT*, UINT*);
    GetDpiForMonitorFn fn = (GetDpiForMonitorFn)(void(*)(void))
        GetProcAddress(GetModuleHandleW(L"shcore.dll"), "GetDpiForMonitor");
    UINT dx = 96, dy = 96;
    if (fn && SUCCEEDED(fn(monitor, 0, &dx, &dy)) && dx) return dx;
    return 96;
}

HMONITOR FindLogicalMonitor(POINT logicalPt, RECT* logicalMonitorRect) {
    HMONITOR monitor = MonitorFromPoint(logicalPt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (monitor && GetMonitorInfoW(monitor, &mi)) {
        *logicalMonitorRect = mi.rcMonitor;
    } else {
        SetRectEmpty(logicalMonitorRect);
    }
    return monitor;
}

BOOL MapLogicalPointToPhysical(HMONITOR monitor, const RECT* logicalMonitorRect, POINT* pt) {
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (!monitor || !GetMonitorInfoW(monitor, &mi)) return FALSE;
    int logicalW = logicalMonitorRect->right - logicalMonitorRect->left;
    int logicalH = logicalMonitorRect->bottom - logicalMonitorRect->top;
    int physicalW = mi.rcMonitor.right - mi.rcMonitor.left;
    int physicalH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    if (logicalW <= 0 || logicalH <= 0 || physicalW <= 0 || physicalH <= 0) return FALSE;
    pt->x = mi.rcMonitor.left + MulDiv(pt->x - logicalMonitorRect->left, physicalW, logicalW);
    pt->y = mi.rcMonitor.top + MulDiv(pt->y - logicalMonitorRect->top, physicalH, logicalH);
    return TRUE;
}

// ===========================================
// 文字
// ===========================================

void DrawTextClearType(HDC hdc, HFONT font, const wchar_t* text, RECT rc, COLORREF color, UINT flags) {
    HGDIOBJ old = SelectObject(hdc, font);
    int oldBk = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, color);
    DrawTextW(hdc, text, -1, &rc, flags | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBk);
    SelectObject(hdc, old);
}

int MeasureTextWidth(HDC hdc, HFONT font, const wchar_t* text) {
    HGDIOBJ old = SelectObject(hdc, font);
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    SelectObject(hdc, old);
    return sz.cx;
}

void SnapshotAlpha(const BYTE* pixels, BYTE* outAlpha, size_t pixelCount) {
    if (!outAlpha) return;
    for (size_t i = 0; i < pixelCount; i++) outAlpha[i] = pixels[i * 4 + 3];
}

void RestoreAlpha(BYTE* pixels, const BYTE* alpha, size_t pixelCount) {
    if (!alpha) return;
    for (size_t i = 0; i < pixelCount; i++) pixels[i * 4 + 3] = alpha[i];
}
