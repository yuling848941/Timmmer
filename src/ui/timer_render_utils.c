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

// ===========================================
// 盒式模糊（背景快照 / 文字辉光共用）
// ===========================================

// 可分离盒式模糊：横向、纵向各做一次滑窗，重复 iterations 次可逼近高斯。
static void BoxBlur(BYTE* dst, const BYTE* src, int w, int h, int radius, int iterations) {
    if (radius <= 0 || w <= 1 || h <= 1) {
        if (dst != src) memcpy(dst, src, (size_t)w * h * 4);
        return;
    }
    if (radius > w - 1) radius = w - 1;
    if (radius > h - 1) radius = h - 1;
    BYTE* tmp = (BYTE*)malloc((size_t)w * h * 4);
    if (!tmp) return;
    memcpy(dst, src, (size_t)w * h * 4);
    for (int it = 0; it < iterations; it++) {
        for (int dir = 0; dir < 2; dir++) {
            const BYTE* in = dst;
            BYTE* out = tmp;
            int outer = dir ? w : h, inner = dir ? h : w;
            int step = dir ? w : 1, lineStep = dir ? 1 : w;
            for (int o = 0; o < outer; o++) {
                int base = o * lineStep, acc[4] = { 0, 0, 0, 0 }, cnt = 0;
                for (int k = -radius; k <= radius; k++) {
                    int i = k;
                    if (i < 0) i = 0;
                    if (i > inner - 1) i = inner - 1;
                    const BYTE* p = in + (size_t)(base + i * step) * 4;
                    for (int c = 0; c < 4; c++) acc[c] += p[c];
                    cnt++;
                }
                for (int i = 0; i < inner; i++) {
                    int add = i + radius;
                    if (add > inner - 1) add = inner - 1;
                    int sub = i - radius - 1;
                    if (sub < 0) sub = 0;
                    const BYTE* pa = in + (size_t)(base + add * step) * 4;
                    const BYTE* ps = in + (size_t)(base + sub * step) * 4;
                    for (int c = 0; c < 4; c++) acc[c] += pa[c] - ps[c];
                    BYTE* q = out + (size_t)(base + i * step) * 4;
                    for (int c = 0; c < 4; c++) q[c] = (BYTE)(acc[c] / cnt);
                }
            }
            memcpy(dst, tmp, (size_t)w * h * 4);
        }
    }
    free(tmp);
}

// ===========================================
// 背景快照与玻璃面板
// ===========================================

BOOL Backdrop_Capture(Backdrop* bd, int x, int y, int w, int h) {
    memset(bd, 0, sizeof(*bd));
    if (w <= 0 || h <= 0) return FALSE;
    HDC screen = GetDC(NULL);
    if (!screen) return FALSE;
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi); bi.biWidth = w; bi.biHeight = -h; bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP dib = CreateDIBSection(screen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    BOOL ok = FALSE;
    if (dc && dib) {
        HGDIOBJ old = SelectObject(dc, dib);
        GdiFlush();
        // CAPTUREBLT：把其它程序的分层窗口也拍进来
        ok = BitBlt(dc, 0, 0, w, h, screen, x, y, SRCCOPY | CAPTUREBLT);
        SelectObject(dc, old);
        if (ok && bits) {
            // DIB 段删除后像素随之失效，所以拷贝一份自持
            bd->pixels = (BYTE*)malloc((size_t)w * h * 4);
            if (bd->pixels) {
                memcpy(bd->pixels, bits, (size_t)w * h * 4);
                bd->x = x; bd->y = y; bd->w = w; bd->h = h;
            } else {
                ok = FALSE;
            }
        }
    }
    if (dib) DeleteObject(dib);
    if (dc) DeleteDC(dc);
    ReleaseDC(NULL, screen);
    return ok;
}

void Backdrop_Release(Backdrop* bd) {
    if (bd->pixels) free(bd->pixels);
    memset(bd, 0, sizeof(*bd));
}

void GlassCache_Free(GlassCache* gc) {
    if (gc->pixels) free(gc->pixels);
    gc->pixels = NULL;
    gc->w = gc->h = 0;
}

BOOL GlassCache_Build(GlassCache* gc, int bufW, int bufH, int px, int py, int pw, int ph, int radius,
                      int shadowSize, int blurRadius, const Backdrop* bd, int screenX, int screenY,
                      const UiTheme* theme) {
    GlassCache_Free(gc);
    if (bufW <= 0 || bufH <= 0 || pw <= 0 || ph <= 0 || !theme) return FALSE;
    gc->pixels = (BYTE*)calloc((size_t)bufW * bufH, 4);
    if (!gc->pixels) return FALSE;
    gc->w = bufW;
    gc->h = bufH;

    // 1) 投影
    DrawSoftShadowSDF(gc->pixels, bufW, bufH, px, py, pw, ph, radius, shadowSize, shadowSize / 6,
                      theme->shadowAlpha / 255.0f);

    // 2) 玻璃底：背景快照中同一屏幕位置的区域做模糊，再按不透明度叠上底色
    float cx = px + pw / 2.0f, cy = py + ph / 2.0f, hw = pw / 2.0f, hh = ph / 2.0f;
    float k = theme->glassAlpha / 255.0f;
    BYTE gr = GetRValue(theme->glass), gg = GetGValue(theme->glass), gb = GetBValue(theme->glass);
    BYTE* blur = NULL;
    if (bd && bd->pixels && bd->w > 0 && bd->h > 0) {
        blur = (BYTE*)malloc((size_t)pw * ph * 4);
        if (blur) {
            // 超出快照范围的部分钳制到快照边缘像素
            int bx = px + screenX - bd->x, by = py + screenY - bd->y;
            for (int j = 0; j < ph; j++) {
                int sy = by + j;
                if (sy < 0) sy = 0;
                if (sy > bd->h - 1) sy = bd->h - 1;
                const BYTE* row = bd->pixels + (size_t)sy * bd->w * 4;
                BYTE* out = blur + (size_t)j * pw * 4;
                for (int i = 0; i < pw; i++) {
                    int sx = bx + i;
                    if (sx < 0) sx = 0;
                    if (sx > bd->w - 1) sx = bd->w - 1;
                    const BYTE* s = row + (size_t)sx * 4;
                    BYTE* d = out + (size_t)i * 4;
                    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
                }
            }
            BoxBlur(blur, blur, pw, ph, blurRadius, 2);
        }
    }
    for (int y = py; y < py + ph; y++) {
        if (y < 0 || y >= bufH) continue;
        for (int x = px; x < px + pw; x++) {
            if (x < 0 || x >= bufW) continue;
            float cov = RoundedRectCoverage(x, y, cx, cy, hw, hh, (float)radius);
            if (cov <= 0.0f) continue;
            float r = gr, g = gg, b = gb;
            if (blur) {
                const BYTE* s = blur + ((size_t)(y - py) * pw + (x - px)) * 4;   // BGRA
                r = s[2] * (1 - k) + gr * k;
                g = s[1] * (1 - k) + gg * k;
                b = s[0] * (1 - k) + gb * k;
            }
            BYTE* o = gc->pixels + ((size_t)y * bufW + x) * 4;                   // 预乘 ARGB
            o[0] = (BYTE)(b * cov);
            o[1] = (BYTE)(g * cov);
            o[2] = (BYTE)(r * cov);
            o[3] = (BYTE)(255 * cov + 0.5f);
        }
    }
    free(blur);

    // 3) 描边与顶部高光
    if (theme->panelBorderAlpha) {
        DrawRoundedRectOutlineAA(gc->pixels, bufW, bufH, radius, radius, px, py, pw, ph, 1,
                                 GetRValue(theme->panelBorder), GetGValue(theme->panelBorder), GetBValue(theme->panelBorder),
                                 theme->panelBorderAlpha);
    }
    if (theme->topHighlightAlpha && radius > 0 && pw > 2 * radius) {
        FillRoundedRectAA(gc->pixels, bufW, bufH, 0, 0, px + radius, py + 1, pw - 2 * radius, 1, 255, 255, 255, theme->topHighlightAlpha);
    }
    return TRUE;
}

void GlassCache_Blit(const GlassCache* gc, BYTE* dst, int bufW, int bufH) {
    if (!gc->pixels || gc->w != bufW || gc->h != bufH) return;
    memcpy(dst, gc->pixels, (size_t)bufW * bufH * 4);
}

// ===========================================
// 辉光
// ===========================================

void GlowRoundedRect(BYTE* pixels, int bufW, int bufH, int rx, int ry, int x, int y, int w, int h,
                     BYTE r, BYTE g, BYTE b, float strength, int spread) {
    (void)ry;
    if (strength <= 0.0f || spread <= 0) return;
    float cx = x + w / 2.0f, cy = y + h / 2.0f, hw = w / 2.0f, hh = h / 2.0f, rad = (float)rx;
    int x0 = x - spread - 1, y0 = y - spread - 1, x1 = x + w + spread + 1, y1 = y + h + spread + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > bufW) x1 = bufW;
    if (y1 > bufH) y1 = bufH;
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            float d = RoundedRectSDF(px + 0.5f, py + 0.5f, cx, cy, hw, hh, rad);
            if (d <= 0.0f || d >= spread) continue;
            float k = 1.0f - d / spread;
            BlendPixel(pixels, (py * bufW + px) * 4, r, g, b, strength * k * k);
        }
    }
}

void DrawTextGlow(BYTE* pixels, int bufW, int bufH, HDC hdc, HFONT font, const wchar_t* text, RECT rc, UINT flags,
                  COLORREF color, float strength, int radius, BOOL additive) {
    if (!text || strength <= 0.0f || radius <= 0) return;
    (void)hdc;
    int tw = rc.right - rc.left, th = rc.bottom - rc.top;
    if (tw <= 0 || th <= 0) return;
    int pad = radius * 2 + 8;
    int w = tw + pad * 2, h = th + pad * 2;

    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi); bi.biWidth = w; bi.biHeight = -h; bi.biPlanes = 1; bi.biBitCount = 32; bi.biCompression = BI_RGB;
    void* bits = NULL;
    HBITMAP dib = CreateDIBSection(screen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    BYTE* mask = (BYTE*)malloc((size_t)w * h * 4);
    BYTE* blur = (BYTE*)malloc((size_t)w * h * 4);
    if (dc && dib && mask && blur) {
        HGDIOBJ oldBmp = SelectObject(dc, dib);
        memset(bits, 0, (size_t)w * h * 4);
        RECT trc = { pad, pad, pad + tw, pad + th };
        DrawTextClearType(dc, font, text, trc, RGB(255, 255, 255), flags);
        GdiFlush();
        // 取 RGB 最大值作为灰度掩码（ClearType 的彩色边缘因此变成灰度）
        memset(mask, 0, (size_t)w * h * 4);
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const BYTE* p = (BYTE*)bits + i * 4;
            BYTE m = p[0];
            if (p[1] > m) m = p[1];
            if (p[2] > m) m = p[2];
            mask[i * 4] = mask[i * 4 + 1] = mask[i * 4 + 2] = m;
            mask[i * 4 + 3] = 255;
        }
        SelectObject(dc, oldBmp);

        // 两级模糊：贴近字形的亮芯 + 向外扩散的柔光
        const float gains[2] = { 0.70f, 0.45f };
        const int radii[2] = { radius / 3 < 1 ? 1 : radius / 3, radius };
        int bx0 = rc.left - pad, by0 = rc.top - pad;
        BYTE cr = GetRValue(color), cg = GetGValue(color), cb = GetBValue(color);
        for (int pass = 0; pass < 2; pass++) {
            BoxBlur(blur, mask, w, h, radii[pass], 2);
            for (int j = 0; j < h; j++) {
                int cy = by0 + j;
                if (cy < 0 || cy >= bufH) continue;
                for (int i = 0; i < w; i++) {
                    int cx = bx0 + i;
                    if (cx < 0 || cx >= bufW) continue;
                    float v = strength * gains[pass] * blur[((size_t)j * w + i) * 4] / 255.0f;
                    if (v <= 0.0f) continue;
                    BYTE* o = pixels + ((size_t)cy * bufW + cx) * 4;
                    if (additive) {
                        int nv = o[2] + (int)(cr * v); o[2] = nv > 255 ? 255 : (BYTE)nv;
                        nv = o[1] + (int)(cg * v); o[1] = nv > 255 ? 255 : (BYTE)nv;
                        nv = o[0] + (int)(cb * v); o[0] = nv > 255 ? 255 : (BYTE)nv;
                    } else {
                        float k = v > 1.0f ? 1.0f : v * 0.55f;
                        o[2] = (BYTE)(o[2] + (cr - o[2]) * k);
                        o[1] = (BYTE)(o[1] + (cg - o[1]) * k);
                        o[0] = (BYTE)(o[0] + (cb - o[0]) * k);
                    }
                }
            }
        }
    }
    free(mask);
    free(blur);
    if (dib) DeleteObject(dib);
    if (dc) DeleteDC(dc);
    ReleaseDC(NULL, screen);
}

// ===========================================
// 其它
// ===========================================

void DrawSwitchThemed(BYTE* pixels, int bufW, int bufH, int x, int y, int rowHeight, BOOL isOn, UINT dpi, const UiTheme* theme) {
    if (!theme) theme = Ui_GetTheme();
    int sw = ScalePx(38, dpi), sh = ScalePx(22, dpi), knob = ScalePx(16, dpi), r = sh / 2;
    int cy = y + (rowHeight - sh) / 2;
    if (isOn) {
        BYTE ar = GetRValue(theme->accentFill), ag = GetGValue(theme->accentFill), ab = GetBValue(theme->accentFill);
        GlowRoundedRect(pixels, bufW, bufH, r, r, x, cy, sw, sh, ar, ag, ab, theme->glow / 255.0f * 0.55f, ScalePx(18, dpi));
        FillRoundedRectAA(pixels, bufW, bufH, r, r, x, cy, sw, sh, ar, ag, ab, 255);
        FillRoundedRectAA(pixels, bufW, bufH, knob / 2, knob / 2, x + sw - knob - ScalePx(3, dpi), cy + ScalePx(3, dpi),
                          knob, knob, 255, 255, 255, 255);
    } else {
        // 关态：凹陷的轨道 + 滑块。亮色主题的白色滑块需要一圈描边才看得见
        BYTE ha = theme->hairAlpha + 13;
        if (ha < theme->hairAlpha || ha > 255) ha = 255;
        FillRoundedRectAA(pixels, bufW, bufH, r, r, x, cy, sw, sh,
                          GetRValue(theme->hair), GetGValue(theme->hair), GetBValue(theme->hair), ha);
        FillRoundedRectAA(pixels, bufW, bufH, knob / 2, knob / 2, x + ScalePx(3, dpi), cy + ScalePx(3, dpi),
                          knob, knob, GetRValue(theme->knobOff), GetGValue(theme->knobOff), GetBValue(theme->knobOff), 255);
        if (!theme->isDark) {
            DrawRoundedRectOutlineAA(pixels, bufW, bufH, knob / 2, knob / 2, x + ScalePx(3, dpi), cy + ScalePx(3, dpi),
                                     knob, knob, 1, GetRValue(theme->hair), GetGValue(theme->hair), GetBValue(theme->hair), 56);
        }
    }
}

void DrawHLine(BYTE* pixels, int bufW, int x0, int x1, int y, BYTE r, BYTE g, BYTE b, BYTE a, int thickness) {
    if (thickness <= 0 || !a) return;
    if (x0 < 0) x0 = 0;
    for (int k = 0; k < thickness; k++) {
        int py = y + k;
        if (py < 0) continue;
        for (int px = x0; px < x1; px++) {
            if (px < 0 || px >= bufW) continue;
            BlendPixel(pixels, (py * bufW + px) * 4, r, g, b, a / 255.0f);
        }
    }
}
